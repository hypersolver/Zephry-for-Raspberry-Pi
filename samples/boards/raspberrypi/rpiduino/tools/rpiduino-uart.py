"""Query RPiduino over UART or validate and stage an AArch64 IMG in RAM."""

import argparse
import re
import time
import zlib
from contextlib import nullcontext
from pathlib import Path

import serial
from serial.tools import list_ports


BOOT = re.compile(r"\*\*\* Booting Zephyr OS build (.+) \*\*\*")
HEARTBEAT = re.compile(r"RPiduino Zephyr: uptime=(\d+) ms")


def capture(port, seconds, log=None, verify=False, version=None):
    deadline = time.monotonic() + seconds
    pending = bytearray()
    boot_version = None
    setups = count = total = 0
    last = None
    with log.open("xb") if log else nullcontext() as output:
        while time.monotonic() < deadline:
            chunk = port.read(min(port.in_waiting or 1, 4096))
            if not chunk:
                continue
            total += len(chunk)
            if output:
                output.write(chunk)
                output.flush()
            pending.extend(chunk)
            while (newline := pending.find(b"\n")) >= 0:
                line = bytes(pending[:newline]).decode("ascii", errors="replace").strip("\x00\r")
                del pending[: newline + 1]
                print(line, flush=True)
                if not verify:
                    continue
                if match := BOOT.fullmatch(line):
                    boot_version = match[1]
                    setups = count = 0
                    last = None
                elif boot_version and line == "RPiduino Zephyr: setup complete":
                    setups += 1
                elif boot_version and setups == 1 and (match := HEARTBEAT.fullmatch(line)):
                    now = int(match[1])
                    count = count + 1 if last is None or 900 <= now - last <= 1200 else 1
                    last = now
                    if count >= 4 and (version is None or boot_version == version):
                        print(f"PASS: {boot_version}, one setup and {count} heartbeats.")
                        return
            if len(pending) > 4096:
                pending.clear()
        if verify:
            raise TimeoutError(
                f"Cold boot not verified in {seconds:g}s "
                f"({total} bytes, boot={boot_version}, setup={setups}, heartbeat={count})"
            )
        print(f"Received {total} bytes.")


def exchange(port, command, expected, timeout=3):
    frame = f"@RPIDUINO {command}\n".encode("ascii")
    # The Pi console polls the mini-UART's eight-byte hardware FIFO.
    for offset in range(0, len(frame), 4):
        port.write(frame[offset : offset + 4])
        port.flush()
        time.sleep(0.004)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = port.readline().decode("ascii", errors="replace").strip()
        if line.startswith("@RPIDUINO ERROR "):
            raise RuntimeError(line)
        if line == expected or (expected.endswith(" ") and line.startswith(expected)):
            return line
    raise TimeoutError(f"No expected RPiduino reply to {command.split()[0]}")


def stage_image(port, image):
    checksum = zlib.crc32(image)
    exchange(port, f"PUT {len(image):08x} {checksum:08x}", "@RPIDUINO OK")
    try:
        for offset in range(0, len(image), 64):
            exchange(port, f"DATA {image[offset : offset + 64].hex()}", "@RPIDUINO OK")
        return exchange(port, "END", f"@RPIDUINO STAGED RAM {len(image)} {checksum:08x}")
    except (OSError, RuntimeError):
        try:
            exchange(port, "CANCEL", "@RPIDUINO OK")
        except (OSError, RuntimeError):
            pass
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port")
    parser.add_argument("--baud", type=int, default=115200)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("ports", help="List serial adapters")
    for name in ("monitor", "verify"):
        action = commands.add_parser(name)
        action.add_argument("--seconds", type=float, default=30 if name == "monitor" else 180)
        action.add_argument("--log", type=Path, help="Exclusive path for the raw UART capture")
        if name == "verify":
            action.add_argument("--version", help="Require an exact boot banner version, e.g. v4.5.0")
    commands.add_parser("ping")
    commands.add_parser("debug")
    reset = commands.add_parser("reset", help="Reboot via UART and optionally verify the next boot")
    reset.add_argument("--verify", action="store_true")
    reset.add_argument("--seconds", type=float, default=90)
    reset.add_argument("--version")
    reset.add_argument("--log", type=Path)
    commands.add_parser("boot-clear", help="Restore the firmware's default IMG on next boot")
    stage = commands.add_parser("stage", help="Copy to volatile RAM and verify CRC32")
    stage.add_argument("image", type=Path)
    update = commands.add_parser("update", help="Write the IMG to microSD and select it for next boot")
    update.add_argument("image", type=Path)
    args = parser.parse_args()

    if args.command == "ports":
        for port in sorted(list_ports.comports()):
            print(f"{port.device}\t{port.description}")
        return
    if not args.port:
        parser.error("--port is required for this command")
    if args.command in ("monitor", "verify", "reset") and args.seconds <= 0:
        parser.error("--seconds must be positive")
    if args.command == "reset" and not args.verify and (args.log or args.version):
        parser.error("--log and --version require reset --verify")
    if args.command == "reset" and args.log and args.log.exists():
        parser.error("--log must name a new file")

    try:
        image = args.image.read_bytes() if args.command in ("stage", "update") else None
        if image is not None and (
            not 64 <= len(image) <= 256 * 1024
            or image[56:60] != b"ARM\x64"
            or image[3] >> 2 != 0x05
        ):
            parser.error("expected a 64-byte to 256 KiB AArch64 kernel IMG")

        with serial.serial_for_url(
            args.port, baudrate=args.baud, timeout=0.25, write_timeout=3
        ) as port:
            if args.command in ("monitor", "verify"):
                print(f"Listening on {args.port} at {args.baud} 8N1.", flush=True)
                capture(
                    port, args.seconds, args.log, args.command == "verify",
                    getattr(args, "version", None),
                )
                return
            print(exchange(port, "PING", "@RPIDUINO READY 1"))
            if args.command == "debug":
                print(exchange(port, "DEBUG", "@RPIDUINO DEBUG "))
            elif args.command == "reset":
                print(exchange(port, "RESET", "@RPIDUINO RESETTING"), flush=True)
                if args.verify:
                    capture(port, args.seconds, args.log, True, args.version)
            elif args.command == "boot-clear":
                print(exchange(port, "BOOT-CLEAR", "@RPIDUINO OK", timeout=30))
            elif image is not None:
                print(stage_image(port, image))
                if args.command == "update":
                    name = f"{zlib.crc32(image):08X}.IMG"
                    print(exchange(port, "SAVE", f"@RPIDUINO SAVED {name}", timeout=30))
                    print(exchange(port, "BOOT", f"@RPIDUINO NEXT {name}", timeout=30))
    except (OSError, RuntimeError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
