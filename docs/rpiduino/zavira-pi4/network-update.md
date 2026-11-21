# Pi 4B TFTP updates and microSD fallback

## Setup

1. Keep a working Pi 4B Zavira microSD installed. Configure the BCM2711 EEPROM with `BOOT_ORDER=0xf12` for Ethernet first and SD fallback. Use `BOOT_UART=1` to capture the EEPROM boot path. [EEPROM configuration](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#raspberry-pi-bootloader-configuration)
2. Provide the TFTP server address through DHCP or set `TFTP_IP`, `CLIENT_IP`, and `SUBNET` in the EEPROM for a static test network. Use a reserved or otherwise unused client address. Fixed IPv4 was used for the hardware test. The Windows TFTP listener and its transfer sockets were bound to the wired interface.
3. Install Python 3.11 or later, `dosfstools`, and `mtools` on the publishing host. Use a dedicated, writable boot directory served by a read-only TFTP service. Run this command from the integration app directory.

```sh
python3 boot/netboot.py --compact rpiduino-zephyr-4.5.0-pi4-zavira-split.zip /srv/tftp/pi4
```

The command validates the ZIP and ZAPP bundle, creates an 8 MiB FAT16 `boot.img` when the payload is up to 6 MiB, and checks every file by reading it back. It atomically replaces `boot.img` on the host after verification. The outer `config.txt` enables `boot_ramdisk=1`. With the default options the tool produces a 36 MiB FAT32 image. Adjust `TFTP_FILE_TIMEOUT` to suit the image size and measured link throughput. To roll back the network version, publish the previous ZIP and reset the Pi. [Raspberry Pi boot ramdisk](https://www.raspberrypi.com/documentation/computers/config_txt.html#boot_ramdisk)

## Hardware verification

Use COM3 at 115200 8N1. On Windows, run the UART tool with pySerial installed.

```powershell
python tools/rpiduino-uart.py --port COM3 reset --verify --version v4.5.0 --seconds 45 --log pi4-tftp.log
```

1. Confirm `Boot mode: NETWORK (02)`, complete transfer of `boot.img`, and a `[ramdisk]` stage. Check the Zephyr v4.5.0 banner, `RPiduino ZAPP: desktop ready`, and four heartbeats. The hardware test downloaded 8,388,608 bytes in about 4.4 seconds.
2. Stop TFTP without disconnecting Ethernet, then repeat the UART reset command with `--seconds 150 --log pi4-fallback.log`. Confirm network attempt and timeouts followed by `Boot mode: SD (01)`, the same Zephyr banner, Zavira desktop, and heartbeats. On the tested board the switch to SD occurred about 90 seconds after reset because EEPROM waited for multiple individual TFTP requests.

The published Pi 4B Zavira ZIP was also tested from microSD with the HDMI LVGL Demo, UART `DEBUG`, and UART `RESET`.

## Benchmark build

Use the same compact TFTP workflow with rpiduino-zephyr-4.5.0-pi4-zavira-benchmark.zip. The updated tool also supports previous two-module boot packages.
