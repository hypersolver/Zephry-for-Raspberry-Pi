# Project overview: Zyphry for Raspberry Pi and Zavira GUI

[繁體中文](project-overview.md) · [Architecture diagrams](architecture.md)

## Motivation

RPiduino brings real-time control and an HDMI interface to Raspberry Pi. The first goal is a repeatable Zephyr microSD boot and UART validation workflow. Zavira then adds an LVGL splash screen, desktop, and applications that can be updated independently of the kernel. The longer-term goal is for native Zephyr applications and Arduino sketches to share GUI services, with platform adapters allowing XDOS and other systems to reuse Zavira themes and widgets.

## System architecture

| Layer | Responsibility |
| --- | --- |
| Raspberry Pi firmware | Loads the Zephyr kernel and the ZAPP bundle from microSD. |
| Zyphry for Raspberry Pi | Board support, display and UART integration, boot packaging, and hardware validation. |
| Shared LVGL and LLEXT | A GUI thread handles rendering and UI operations. LLEXT loads ZAPP modules using the host ABI. |
| Zavira Desktop and applications | `desktop.zapp` provides the animated pinwheel splash, version and uptime display, and Launcher. The Launcher can start `lvgl-demo.zapp`. |
| Application API (planned) | A shared API for native applications and a C++ wrapper for Arduino sketches using `setup()` and `loop()`. |

The Pi 4B prototype packages both modules in `zapps.bin`. The modules define the desktop and Demo layouts, while the kernel provides a shared LVGL runtime. COM3 provides heartbeat output, `DEBUG`, and `RESET`. The prototype also accepts `OPEN LVGL` and `DESKTOP` for navigation.

## Development workflow

1. **Maintain the base system.** Keep the Raspberry Pi fork's `main` aligned with upstream Zephyr. Use version-pinned validation branches for board builds and verify cold boot, UART heartbeat, `DEBUG`, and `RESET`.
2. **Develop Zavira independently.** The planned `zavira-gui` repository will use `main` for the core, theme, desktop, demo, and platform adapters. Use short-lived branches and pull requests, validate UI changes in simulation, and tag Zavira releases independently.
3. **Integrate and validate.** Pin compatible Zephyr, LVGL, and Zavira revisions on `rpiduino/zavira-gui`. Verify boot, splash animation, Launcher, Demo switching, and UART in QEMU, then check HDMI, cold boot, and reboot-to-desktop on the Pi 4B.
4. **Publish by board and purpose.** Publish base and GUI boot packages with a board name, software version, validation scope, and SHA-256 checksum. Pi 3B and 3B+ share a base package. Pi 4B has a separate image. Add Pi 5 after hardware validation.

## Future applications

| Application | First milestone |
| --- | --- |
| Embedded control panel | Display sensor readings, device status, and alerts on HDMI with UART diagnostics. |
| Arduino graphics | Run a sketch on Zephyr that updates Zavira widgets, then validate native module loading, unloading, and GUI resource cleanup. |
| Diagnostics and demonstrations | Package self-tests, system information, and interactive demos as separate ZAPPs. |
| Cross-platform GUI | Reuse the pinwheel, theme, and desktop widgets through PC simulator and XDOS platform adapters. |
| Raspberry Pi 5 | Validate base boot and UART before integrating display and input. |

## Validation status

Base boot and UART heartbeat have been exercised on Pi 3B, Pi 3B+, and Pi 4B. The Zavira animation, Launcher, Demo switching, heartbeat, and `DEBUG` have been verified in QEMU. HDMI and `RESET` for the Zavira image still need Pi 4B hardware validation. The independent Zavira repository, Arduino API, USB input, and Pi 5 validation are planned. The base boot package release is currently a GitHub draft.
