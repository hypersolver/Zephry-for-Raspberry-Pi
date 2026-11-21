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
FIRMWARE_FILES = ("bootcode.bin", "start.elf", "fixup.dat", "LICENCE.broadcom")
BOOT_FILES = Path(__file__).resolve().parent


def package(build_dir: Path, output: Path) -> None:
    zephyr_dir = build_dir / "zephyr"
    config = (zephyr_dir / ".config").read_text(encoding="utf-8").splitlines()
    boot_config = (BOOT_FILES / "config.txt").read_text(encoding="utf-8").splitlines()
    if (
        'CONFIG_SOC="bcm2837"' not in config
        or "CONFIG_SRAM_BASE_ADDRESS=0x200000" not in config
        or "kernel_address=0x200000" not in boot_config
        or "core_freq=250" not in boot_config
    ):
        raise ValueError("Expected the rpi_3 mini-UART boot configuration at 0x200000")

    kernel = (zephyr_dir / "zephyr.bin").read_bytes()
    if kernel[56:60] != b"ARM\x64" or struct.unpack_from("<I", kernel)[0] >> 26 != 0b000101:
        raise ValueError("Expected an AArch64 boot image with an entry branch")

    output.parent.mkdir(parents=True, exist_ok=True)
    with NamedTemporaryFile(dir=output.parent, delete=False) as temp:
        archive = Path(temp.name)
    try:
        with ZipFile(archive, "w", ZIP_DEFLATED) as boot:
            if "CONFIG_FAT_FILESYSTEM_ELM=y" in config:
                boot_config.append("include rpinext.txt")
            boot.writestr("config.txt", "\n".join(boot_config) + "\n")
            boot.writestr("kernel8-rpiduino.img", kernel)
            for name in FIRMWARE_FILES:
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
    args = parser.parse_args()
    package(args.build_dir, args.output)
