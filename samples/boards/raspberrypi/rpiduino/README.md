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

UART RESET requires adapter TX to physical pin 10. Run the UART tool with reset --verify to collect the next boot on the same port.

QEMU raspi3b confirmed a guest reset, fresh setup, and four heartbeats. Physical v4.5.0 validation is pending.

## Additional validation builds

- Pi 4B and Pi 5 use the upstream rpi_4b and rpi_5 targets. Pass --board rpi_4b or --board rpi_5 to the packaging script and place the ZIP files on an MBR/FAT32 microSD. Pi 4B uses GPIO14/15, while Pi 5 exposes its console on the dedicated three-pin debug connector.
- For the Pi 3 UART updater, run west update fatfs, then build with -DRPIDUINO_SKETCH=uart_debug. With Ubuntu cross GCC, add -fno-stack-protector to EXTRA_CXXFLAGS for this bare-metal configuration.
- UART update sends the image, writes and reads it back from FAT32, and selects it via RPINEXT.TXT. RESET reboots the board. BOOT-CLEAR removes the selection so the firmware uses the original kernel.
- Pi 3 GPIO loopback uses -DRPIDUINO_SKETCH=gpio_loopback. Connect GPIO17 (header pin 11) to GPIO27 (header pin 13) with the board powered off.

Zephyr v4.5.0 passed the QEMU Pi 3 boot, guest RESET, MBR/FAT32 write/readback,
persistence across RESET, and rollback checks. Pi 4B and Pi 5 builds and ZIP
checks passed. Physical v4.5.0 acceptance remains pending for all four boards.
