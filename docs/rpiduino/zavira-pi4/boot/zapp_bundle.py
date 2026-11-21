"""Pack Zavira and its LVGL applications as a Pi firmware RAM payload."""

import argparse
import struct
from pathlib import Path
from zlib import crc32


NAMES = ("desktop.zapp", "lvgl-demo.zapp", "lvgl-benchmark.zapp")
CAPACITY = 128 * 1024


def bundle_names(abi_version: int) -> tuple[str, ...]:
    if abi_version in (1, 2):
        return NAMES[:2]
    if abi_version == 3:
        return NAMES
    raise ValueError(f"Unsupported ZAPP bundle version: {abi_version}")


def make_bundle(modules: list[bytes], *, abi_version: int = 3) -> bytes:
    if len(modules) != len(bundle_names(abi_version)):
        raise ValueError("Unexpected ZAPP module count")
    header = bytearray(struct.pack("<4sI", b"ZAPP", abi_version))
    payload = bytearray()
    for name, data in zip(bundle_names(abi_version), modules, strict=True):
        if (len(data) < 64 or data[:6] != b"\x7fELF\x02\x01"
                or struct.unpack_from("<HH", data, 16) != (1, 183)):
            raise ValueError(f"{name} must be a little-endian AArch64 relocatable ELF")
        header.extend(struct.pack("<II", len(data), crc32(data)))
        payload.extend(data)
        payload.extend(bytes(-len(data) % 8))
    if len(header) + len(payload) > CAPACITY:
        raise ValueError(f"ZAPP bundle exceeds {CAPACITY} bytes")
    return bytes(header + payload)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_bytes(make_bundle([(args.build_dir / name).read_bytes() for name in NAMES]))
