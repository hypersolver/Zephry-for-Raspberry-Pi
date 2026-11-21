"""Publish a Pi 4B Zephyr boot ZIP to a Raspberry Pi EEPROM netboot directory."""

import argparse
import hashlib
import os
import shutil
import subprocess
from pathlib import Path
from tempfile import TemporaryDirectory
from zipfile import BadZipFile, ZipFile

from zapp_bundle import NAMES, make_bundle


BASE_FILES = {"config.txt", "zephyr.bin", "bcm2711-rpi-4-b.dtb", "start4.elf",
              "fixup4.dat", "LICENCE.broadcom"}
ZAPP_FILES = {*NAMES, "zapps.bin"}
OUTER_CONFIG = b"[all]\nboot_ramdisk=1\n"
IMAGE_SIZE = 36 * 1024 * 1024
COMPACT_IMAGE_SIZE = 8 * 1024 * 1024
COMPACT_MAX_PAYLOAD = 6 * 1024 * 1024
MAX_PAYLOAD = 16 * 1024 * 1024


def extract_package(package: Path, destination: Path) -> list[str]:
    with ZipFile(package) as archive:
        entries = archive.infolist()
        names = {entry.filename for entry in entries}
        if len(names) != len(entries) or names not in (BASE_FILES, BASE_FILES | ZAPP_FILES):
            raise ValueError("Expected a Pi 4B base or Zavira boot ZIP")
        if any(entry.file_size == 0 for entry in entries):
            raise ValueError("A required boot file is empty")
        if sum(entry.file_size for entry in entries) > MAX_PAYLOAD:
            raise ValueError("Pi 4B ZIP exceeds the supported boot payload size")
        config = archive.read("config.txt").decode("ascii").splitlines()
        if not {"arm_64bit=1", "kernel=zephyr.bin", "kernel_address=0x200000"} <= set(config):
            raise ValueError("Unexpected Pi 4B load configuration")
        kernel = archive.read("zephyr.bin")
        if kernel[56:60] != b"ARM\x64" or int.from_bytes(kernel[:4], "little") >> 26 != 0b000101:
            raise ValueError("Expected an AArch64 boot image with an entry branch")
        if ZAPP_FILES <= names:
            if not {"ramfsfile=zapps.bin", "ramfsaddr=0x02000000"} <= set(config):
                raise ValueError("Unexpected ZAPP bundle load configuration")
            bundle = archive.read("zapps.bin")
            abi_version = int.from_bytes(bundle[4:8], "little")
            if make_bundle([archive.read(name) for name in NAMES], abi_version=abi_version) != bundle:
                raise ValueError("ZAPP modules do not match zapps.bin")
        for name in sorted(names):
            with archive.open(name) as source, (destination / name).open("wb") as output:
                shutil.copyfileobj(source, output)
    return sorted(names)


def publish(package: Path, target: Path, *, compact: bool = False) -> str:
    config = target / "config.txt"
    if config.exists() and config.read_bytes() != OUTER_CONFIG:
        raise ValueError("The destination already contains a different config.txt")
    with TemporaryDirectory(prefix=".rpiduino-", dir=target) as directory:
        staging = Path(directory)
        names = extract_package(package, staging)
        image_size, fat_type = IMAGE_SIZE, "32"
        if compact:
            if sum((staging / name).stat().st_size for name in names) > COMPACT_MAX_PAYLOAD:
                raise ValueError("The compact boot image supports up to 6 MiB of files")
            image_size, fat_type = COMPACT_IMAGE_SIZE, "16"
        image = staging / "boot.img"
        with image.open("wb") as output:
            output.truncate(image_size)
        cluster_options = ["-s", "1"] if compact else []
        subprocess.run(["mkfs.fat", "-F", fat_type, *cluster_options, str(image)], check=True,
                       stdout=subprocess.DEVNULL)
        subprocess.run(["mcopy", "-i", str(image), *(str(staging / n) for n in names),
                        "::/"], check=True)
        checked = staging / "readback"
        checked.mkdir()
        subprocess.run(["mcopy", "-i", str(image), *(f"::/{n}" for n in names),
                        str(checked)], check=True)
        for name in names:
            if (checked / name).read_bytes() != (staging / name).read_bytes():
                raise ValueError(f"FAT readback failed for {name}")
        with image.open("rb+") as output:
            digest = hashlib.file_digest(output, "sha256").hexdigest()
            os.fsync(output.fileno())
        # One replacement switches the entire kernel and ZAPP bundle together.
        image.replace(target / "boot.img")
        if not config.exists():
            outer = staging / "netboot-config.txt"
            outer.write_bytes(OUTER_CONFIG)
            outer.replace(config)
    return digest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path, help="Pi 4B boot ZIP")
    parser.add_argument("target", type=Path, help="Existing dedicated TFTP boot directory")
    parser.add_argument("--compact", action="store_true", help="Use an 8 MiB FAT16 boot image")
    args = parser.parse_args()
    try:
        digest = publish(args.package.resolve(), args.target.resolve(), compact=args.compact)
    except (OSError, ValueError, BadZipFile, subprocess.CalledProcessError) as error:
        parser.exit(1, f"netboot publish failed: {error}\n")
    print(f"{digest}  {args.target / 'boot.img'}")
