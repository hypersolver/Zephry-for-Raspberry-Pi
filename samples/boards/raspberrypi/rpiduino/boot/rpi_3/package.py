"""Bundle a built Pi 3 Zephyr kernel and the matching Raspberry Pi firmware."""

import argparse
import hashlib
import struct
from pathlib import Path
from tempfile import NamedTemporaryFile
from urllib.request import urlopen
from zipfile import ZIP_DEFLATED, ZipFile


FIRMWARE_REVISION = "676efed1194de38975889a34276091da1f5aadd3"  # 1.20250915
FIRMWARE_URL = (
    "https://raw.githubusercontent.com/raspberrypi/firmware/"
    f"{FIRMWARE_REVISION}/boot/"
)
BOOT_DIR = Path(__file__).resolve().parent.parent
TARGETS = {
    "rpi_3": ("bcm2837", "kernel8-rpiduino.img", ("bootcode.bin", "start.elf", "fixup.dat")),
    "rpi_4b": ("bcm2711", "zephyr.bin", ("bcm2711-rpi-4-b.dtb", "start4.elf", "fixup4.dat")),
    "rpi_5": ("bcm2712", "zephyr.bin", ("bcm2712-rpi-5-b.dtb",)),
}


def package(build_dir: Path, output: Path, board: str = "rpi_3") -> None:
    zephyr_dir = build_dir / "zephyr"
    config = (zephyr_dir / ".config").read_text(encoding="utf-8").splitlines()
    soc, image_name, firmware_files = TARGETS[board]
    boot_config = (BOOT_DIR / board / "config.txt").read_text(encoding="utf-8").splitlines()
    if f'CONFIG_SOC="{soc}"' not in config or f"kernel={image_name}" not in boot_config:
        raise ValueError(f"Build and configuration must match {board}")
    for line in boot_config:
        if line.startswith("kernel_address="):
            address = int(line.split("=")[1], 0)
            if f"CONFIG_SRAM_BASE_ADDRESS={address:#x}" not in config:
                raise ValueError(f"Build and firmware must agree on load address {address:#x}")
    if board == "rpi_3" and "core_freq=250" not in boot_config:
        raise ValueError("Expected the rpi_3 mini-UART clock at 250 MHz")

    kernel = (zephyr_dir / "zephyr.bin").read_bytes()
    if kernel[56:60] != b"ARM\x64" or struct.unpack_from("<I", kernel)[0] >> 26 != 0b000101:
        raise ValueError("Expected an AArch64 boot image with an entry branch")

    output.parent.mkdir(parents=True, exist_ok=True)
    with NamedTemporaryFile(dir=output.parent, delete=False) as temp:
        archive = Path(temp.name)
    try:
        with ZipFile(archive, "w", ZIP_DEFLATED) as boot:
            if board == "rpi_3" and "CONFIG_FAT_FILESYSTEM_ELM=y" in config:
                boot_config.append("include rpinext.txt")
            boot.writestr("config.txt", "\n".join(boot_config) + "\n")
            boot.writestr(image_name, kernel)
            for name in (*firmware_files, "LICENCE.broadcom"):
                with urlopen(FIRMWARE_URL + name, timeout=45) as firmware:
                    boot.writestr(name, firmware.read())
        archive.replace(output)
    finally:
        archive.unlink(missing_ok=True)

    print(f"{hashlib.sha256(output.read_bytes()).hexdigest()}  {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--board", choices=TARGETS, default="rpi_3")
    args = parser.parse_args()
    package(args.build_dir, args.output, args.board)
