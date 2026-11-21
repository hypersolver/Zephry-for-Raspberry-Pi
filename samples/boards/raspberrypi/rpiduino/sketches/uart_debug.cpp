#include <rpiduino/sketch.hpp>
#include <rpiduino/image_update.hpp>
#include <rpiduino/uart_control.hpp>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

namespace {
const device *const serial = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
constexpr char prefix[] = "@RPIDUINO ";
constexpr uint32_t transferTimeout = 10000;
uint8_t image[256 * 1024];
char command[160];
unsigned char receivedByte;
size_t lineLength;
bool overflow;
size_t expectedSize;
size_t received;
uint32_t expectedCrc;
uint32_t lastActivity;
bool receiving;
bool staged;

void resetTransfer()
{
    expectedSize = 0;
    received = 0;
    receiving = false;
    staged = false;
}

int hexDigit(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

bool readHex32(const char *text, uint32_t &value)
{
    value = 0;
    for (size_t i = 0; i < 8; ++i) {
        const int digit = hexDigit(text[i]);
        if (digit < 0) {
            return false;
        }
        value = (value << 4) | static_cast<uint32_t>(digit);
    }
    return true;
}

void beginTransfer(const char *args)
{
    uint32_t size;
    uint32_t checksum;
    if (receiving || strlen(args) != 17 || args[8] != ' ' ||
        !readHex32(args, size) || !readHex32(args + 9, checksum) ||
        size < 64 || size > sizeof(image)) {
        printk("@RPIDUINO ERROR PUT\n");
        return;
    }
    resetTransfer();
    expectedSize = size;
    expectedCrc = checksum;
    receiving = true;
    lastActivity = millis();
    printk("@RPIDUINO OK\n");
}

void receiveData(const char *hex)
{
    const size_t chars = strlen(hex);
    if (!receiving || chars == 0 || chars % 2 != 0 ||
        chars / 2 > expectedSize - received) {
        printk("@RPIDUINO ERROR DATA\n");
        return;
    }
    for (size_t i = 0; i < chars; i += 2) {
        const int high = hexDigit(hex[i]);
        const int low = hexDigit(hex[i + 1]);
        if (high < 0 || low < 0) {
            printk("@RPIDUINO ERROR DATA\n");
            return;
        }
        image[received + i / 2] = static_cast<uint8_t>((high << 4) | low);
    }
    received += chars / 2;
    lastActivity = millis();
    printk("@RPIDUINO OK\n");
}

void finishTransfer()
{
    if (!receiving || received != expectedSize ||
        crc32_ieee(image, received) != expectedCrc ||
        memcmp(image + 56, "ARM\x64", 4) != 0 ||
        sys_get_le32(image) >> 26 != 0x05) {
        resetTransfer();
        printk("@RPIDUINO ERROR IMG\n");
        return;
    }
    receiving = false;
    staged = true;
    printk("@RPIDUINO STAGED RAM %u %08x\n", static_cast<unsigned int>(received),
           static_cast<unsigned int>(expectedCrc));
}

void dispatch(const char *line)
{
    if (strcmp(line, "PING") == 0) {
        printk("@RPIDUINO READY 1\n");
    } else if (strcmp(line, "DEBUG") == 0) {
        printk("@RPIDUINO DEBUG board=%s uptime_ms=%u img=%s\n",
               CONFIG_BOARD_TARGET, static_cast<unsigned int>(millis()),
               staged ? "staged-ram" : receiving ? "receiving" : "empty");
    } else if (strncmp(line, "PUT ", 4) == 0) {
        beginTransfer(line + 4);
    } else if (strncmp(line, "DATA ", 5) == 0) {
        receiveData(line + 5);
    } else if (strcmp(line, "END") == 0) {
        finishTransfer();
    } else if (strcmp(line, "CANCEL") == 0) {
        resetTransfer();
        printk("@RPIDUINO OK\n");
    } else if (strcmp(line, "RESET") == 0) {
        if (receiving) {
            printk("@RPIDUINO ERROR TRANSFER\n");
        } else {
            rebootFromUart();
        }
    } else if (strcmp(line, "SAVE") == 0 || strcmp(line, "BOOT") == 0) {
        if (!staged) {
            printk("@RPIDUINO ERROR NO-STAGED-IMG\n");
            return;
        }
        const bool select = line[0] == 'B';
        const int result = select ? selectImage(received, expectedCrc)
                                  : saveImage(image, received, expectedCrc);
        if (result < 0) {
            printk("@RPIDUINO ERROR SD %d\n", result);
        } else {
            printk("@RPIDUINO %s %08X.IMG\n", select ? "NEXT" : "SAVED",
                   static_cast<unsigned int>(expectedCrc));
        }
    } else if (strcmp(line, "BOOT-CLEAR") == 0) {
        const int result = clearImageSelection();
        if (result < 0) {
            printk("@RPIDUINO ERROR SD %d\n", result);
        } else {
            printk("@RPIDUINO OK\n");
        }
    } else {
        printk("@RPIDUINO ERROR COMMAND\n");
    }
}
}

void setup()
{
    printk("RPiduino UART debug ready\n");
}

void loop()
{
    if (receiving && millis() - lastActivity >= transferTimeout) {
        resetTransfer();
    }

    while (uart_poll_in(serial, &receivedByte) == 0) {
        const unsigned char ch = receivedByte;
        if (ch == '\n') {
            command[lineLength] = '\0';
            if (overflow) {
                printk("@RPIDUINO ERROR LINE\n");
            } else if (strncmp(command, prefix, sizeof(prefix) - 1) == 0) {
                dispatch(command + sizeof(prefix) - 1);
            }
            lineLength = 0;
            overflow = false;
        } else if (ch != '\r') {
            if (lineLength < sizeof(command) - 1 && ch >= 32 && ch <= 126) {
                command[lineLength++] = static_cast<char>(ch);
            } else {
                overflow = true;
            }
        }
    }
    k_sleep(K_TICKS(1));
}
