# Zavira on Raspberry Pi 4B

This app hosts the Zavira desktop and LVGL Demo as separate AArch64 LLEXT modules. The board overlay, UART protocol, reboot handler, LVGL host, and microSD packaging stay here. CMake fetches the exact [Zavira source revision](https://github.com/hypersolver/zavira-gui/tree/39a7f1fb999ecb014d305ac06a273a477d79ee4f) used by the host.

## Build a microSD boot package

Use the Zephyr 4.5 workspace on this branch with the manifest's LVGL module installed. From the Zephyr repository root, using the Zephyr SDK:

```sh
west update
west build -b rpi_4b -d build-zavira docs/rpiduino/zavira-pi4
python3 docs/rpiduino/zavira-pi4/boot/rpi_3/package.py \
  build-zavira rpiduino-zephyr-4.5.0-pi4-zavira.zip --board rpi_4b
```

With the Ubuntu AArch64 GCC cross toolchain, set `ZEPHYR_TOOLCHAIN_VARIANT=cross-compile` and `CROSS_COMPILE=/usr/bin/aarch64-linux-gnu-`. Pass `-DEXTRA_CFLAGS=-mno-outline-atomics` and `'-DEXTRA_CXXFLAGS=-mno-outline-atomics -fno-stack-protector'` after `--` to west. For an offline build, use `-DFETCHCONTENT_SOURCE_DIR_ZAVIRA=/path/to/zavira-gui` with a checkout of the pinned revision.

Extract the ZIP to the FAT32 boot partition root. The Pi firmware loads `zephyr.bin` and `zapps.bin` using `config.txt`. The splash should give way to the Launcher. UART is 115200, 8N1. Use `tools/rpiduino-uart.py --port COM3 open-lvgl` or `desktop` to navigate. The heartbeat, `debug`, and `reset --verify --version v4.5.0` use the existing UART protocol.

[Project overview](../project-overview.en.md) · [Architecture diagrams](../architecture.md)
