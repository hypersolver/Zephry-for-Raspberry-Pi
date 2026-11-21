#include <rpiduino/uart_control.hpp>
#include <rpiduino/sketch.hpp>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>
#ifdef RPIDUINO_ZAPP_DESKTOP
#include <rpiduino/zapp.h>
#endif

namespace {
const device *const console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
char line[32];
size_t length;
bool overflow;
unsigned char received;
}

[[noreturn]] void rebootFromUart()
{
    printk("@RPIDUINO RESETTING\n");
    // Let the UART finish transmitting before resetting its clock.
    delay(50);
    sys_reboot(SYS_REBOOT_COLD);
}

void pollUartControl()
{
    while (uart_poll_in(console, &received) == 0) {
        if (received == '\n') {
            if (!overflow) {
                line[length] = '\0';
                if (strcmp(line, "@RPIDUINO RESET") == 0) {
                    rebootFromUart();
                } else if (strcmp(line, "@RPIDUINO PING") == 0) {
                    printk("@RPIDUINO READY 1\n");
                } else if (strcmp(line, "@RPIDUINO DEBUG") == 0) {
                    printk("@RPIDUINO DEBUG board=%s uptime_ms=%u img=unsupported\n",
                           CONFIG_BOARD_TARGET, static_cast<unsigned int>(millis()));
#ifdef RPIDUINO_ZAPP_DESKTOP
                } else if (strcmp(line, "@RPIDUINO OPEN LVGL") == 0) {
                    zapp_request(ZAPP_DEMO);
                    printk("@RPIDUINO QUEUED\n");
                } else if (strcmp(line, "@RPIDUINO DESKTOP") == 0) {
                    zapp_request(ZAPP_DESKTOP);
                    printk("@RPIDUINO QUEUED\n");
#endif
                }
            }
            length = 0;
            overflow = false;
        } else if (received != '\r') {
            if (length < sizeof(line) - 1 && received >= 32 && received <= 126) {
                line[length++] = static_cast<char>(received);
            } else {
                overflow = true;
            }
        }
    }
}
