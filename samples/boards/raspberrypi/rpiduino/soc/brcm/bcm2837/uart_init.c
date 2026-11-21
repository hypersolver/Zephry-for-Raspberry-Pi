/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/arch/cpu.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/util.h>

#define UART_NODE DT_NODELABEL(uart1)
#define AUX_BASE DT_REG_ADDR(UART_NODE)
#define AUX_ENABLES (AUX_BASE + 0x04U)
#define AUX_ENABLE_MINI_UART BIT(0)
#define MU_IO (AUX_BASE + 0x40U)
#define MU_IER (AUX_BASE + 0x44U)
#define MU_IIR (AUX_BASE + 0x48U)
#define MU_LCR (AUX_BASE + 0x4cU)
#define MU_MCR (AUX_BASE + 0x50U)
#define MU_LSR (AUX_BASE + 0x54U)
#define MU_CNTL (AUX_BASE + 0x60U)
#define MU_BAUD (AUX_BASE + 0x68U)
#define MU_LCR_8_BITS 3U
#define MU_IIR_CLEAR_FIFOS (BIT(1) | BIT(2))
#define MU_LSR_RX_READY BIT(0)
#define MU_LSR_TX_READY BIT(5)
#define MU_CNTL_RX_TX (BIT(0) | BIT(1))
#define MU_DIVIDER DIV_ROUND_CLOSEST(DT_PROP(UART_NODE, clock_frequency), \
				     8U * DT_PROP(UART_NODE, current_speed))
#define GPFSEL1 (DT_REG_ADDR(DT_NODELABEL(gpio)) + 0x04U)
#define GPIO14_15_MASK (GENMASK(14, 12) | GENMASK(17, 15))
#define GPIO14_15_ALT5 ((2U << 12) | (2U << 15))

BUILD_ASSERT(MU_DIVIDER >= 1U && MU_DIVIDER <= 65536U);

static int mini_uart_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	sys_write32(sys_read32(AUX_ENABLES) | AUX_ENABLE_MINI_UART, AUX_ENABLES);
	sys_write32(0U, MU_CNTL);
	sys_write32(0U, MU_IER);
	sys_write32(MU_LCR_8_BITS, MU_LCR);
	sys_write32(0U, MU_MCR);
	sys_write32(MU_IIR_CLEAR_FIFOS, MU_IIR);
	sys_write32(MU_DIVIDER - 1U, MU_BAUD);
	sys_write32((sys_read32(GPFSEL1) & ~GPIO14_15_MASK) | GPIO14_15_ALT5, GPFSEL1);
	sys_write32(MU_CNTL_RX_TX, MU_CNTL);
	return 0;
}

static int mini_uart_poll_in(const struct device *dev, unsigned char *character)
{
	ARG_UNUSED(dev);

	if ((sys_read32(MU_LSR) & MU_LSR_RX_READY) == 0U) {
		return -1;
	}

	*character = (unsigned char)sys_read32(MU_IO);
	return 0;
}

static void mini_uart_poll_out(const struct device *dev, unsigned char character)
{
	ARG_UNUSED(dev);

	while ((sys_read32(MU_LSR) & MU_LSR_TX_READY) == 0U) {
	}

	sys_write32(character, MU_IO);
}

static DEVICE_API(uart, mini_uart_api) = {
	.poll_in = mini_uart_poll_in,
	.poll_out = mini_uart_poll_out,
};

DEVICE_DT_DEFINE(UART_NODE, mini_uart_init, NULL, NULL, NULL,
		 PRE_KERNEL_1, CONFIG_SERIAL_INIT_PRIORITY, &mini_uart_api);
