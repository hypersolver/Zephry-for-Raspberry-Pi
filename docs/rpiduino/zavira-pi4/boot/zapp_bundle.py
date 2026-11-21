"""Pack the desktop loader and LVGL Demo as a Pi firmware RAM payload."""

import argparse
import struct
from pathlib import Path
from zlib import crc32


NAMES = ("desktop.zapp", "lvgl-demo.zapp")
CAPACITY = 128 * 1024


def make_bundle(modules: list[bytes]) -> bytes:
    header = bytearray(struct.pack("<4sI", b"ZAPP", 2))
    payload = bytearray()
    for name, data in zip(NAMES, modules, strict=True):
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
