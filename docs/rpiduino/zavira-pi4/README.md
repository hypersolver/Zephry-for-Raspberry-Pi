# Zavira on Raspberry Pi 4B

This app hosts the Zavira desktop and LVGL Demo as separate AArch64 LLEXT modules. The board overlay, UART protocol, reboot handler, LVGL host, and microSD packaging stay here. CMake fetches the exact [Zavira source revision](https://github.com/hypersolver/zavira-gui/tree/6f89905e562c661b2ae0b5f36d9a04da5b389b34) used by the host.

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

[TFTP updates and microSD fallback verification](network-update.md)

## COM3 input and benchmark

Install pySerial and use Python with Tk. Run:

    python tools/rpiduino-uart.py --port COM3 gui-input

Move or click in the Windows input window while watching the Pi HDMI display. Tab and Shift+Tab move focus, Enter opens the selected ZAPP, and Esc returns to the Launcher. Close the window to release COM3. Direct USB-A input requires a Pi 4B VL805 xHCI and USB HID host driver.

The Launcher can open the LVGL Demo and LVGL Benchmark. To start the benchmark directly, run:

    python tools/rpiduino-uart.py --port COM3 open-benchmark

**Pi 4B hardware result:** 300 completed LVGL redraws over 10,018 ms, **29.95 redraws/s** for a workload of 24 animated triangles. The COM3 result was RPiduino BENCH: redraws=300 elapsed_ms=10018. Hardware testing also confirmed TFTP boot, the HDMI animation and Launcher, COM3 keyboard and pointer navigation, and the LVGL Demo.

[Download the hardware-tested Benchmark package](https://github.com/hypersolver/Zephry-for-Raspberry-Pi/releases/tag/rpiduino-v4.5.0-pi4-zavira-benchmark)
