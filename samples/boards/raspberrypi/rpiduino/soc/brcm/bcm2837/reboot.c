/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/arch/cpu.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/barrier.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/util.h>

#define PM_BASE DT_REG_ADDR(DT_NODELABEL(power))
#define PM_RSTC (PM_BASE + 0x1cU)
#define PM_WDOG (PM_BASE + 0x24U)
#define PM_PASSWORD 0x5a000000U
#define PM_CONTROL_MASK GENMASK(23, 0)
#define PM_RESET_MODE_MASK GENMASK(5, 4)
#define PM_FULL_RESET BIT(5)
#define PM_RESET_DELAY_TICKS 10U

void sys_arch_reboot(int type)
{
	ARG_UNUSED(type);

	const uint32_t control = sys_read32(PM_RSTC) & PM_CONTROL_MASK;

	sys_write32(PM_PASSWORD | PM_RESET_DELAY_TICKS, PM_WDOG);
	sys_write32(PM_PASSWORD | (control & ~PM_RESET_MODE_MASK) | PM_FULL_RESET, PM_RSTC);
	barrier_dsync_fence_full();

	/* Keep the CPU here until the watchdog resets the SoC. */
	for (;;) {
		__asm__ volatile("wfe");
	}
}
