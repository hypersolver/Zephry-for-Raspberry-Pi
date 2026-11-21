# Zephry for Raspberry Pi — Pi 3 validation

This sample adds a Raspberry Pi 3 board target using BCM2837, the ARM-local
virtual timer, and the mini-UART on GPIO14/15. It boots an AArch64 image at
0x200000 and prints an uptime heartbeat. The fork branch is based on Zephyr v4.5.0.

## Build

From a west workspace using this fork as its Zephyr repository:

```sh
west build -b rpi_3 -d build-pi3 zephyr/samples/boards/raspberrypi/rpiduino
python zephyr/samples/boards/raspberrypi/rpiduino/boot/rpi_3/package.py build-pi3 pi3.zip
```

If using Ubuntu's cross compiler with `ZEPHYR_TOOLCHAIN_VARIANT=cross-compile`,
export `CROSS_COMPILE=/usr/bin/aarch64-linux-gnu-` and add
`-- -DEXTRA_CFLAGS=-mno-outline-atomics -DEXTRA_CXXFLAGS=-mno-outline-atomics`
to the build command.

Extract the ZIP to the root of a spare FAT32 microSD boot partition.
Use a 3.3 V USB–TTL adapter. Connect adapter RX to physical pin 8 (GPIO14)
and adapter GND to physical pin 6. Start the monitor before powering on:

```sh
python -m pip install pyserial
python zephyr/samples/boards/raspberrypi/rpiduino/tools/rpiduino-uart.py --port COM3 verify --log pi3-boot.log
```

Expected output is a Zephyr boot banner, one setup line, and four approximately
one-second heartbeats. The v4.5.0 candidate passed this check with QEMU raspi3b.
Physical v4.5.0 acceptance is ordered Pi 3B, Pi 3B+, Pi 4B, then Pi 5.
The earlier v4.4.1 Pi 3 heartbeats were confirmed on Pi 3B and Pi 3B+.
