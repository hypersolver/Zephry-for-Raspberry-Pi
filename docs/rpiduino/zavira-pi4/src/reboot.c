/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/arch/cpu.h>
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/sys/barrier.h>
#include <zephyr/sys/device_mmio.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/util.h>

#define PM_NODE DT_NODELABEL(power)
#define PM_RSTC 0x1cU
#define PM_WDOG 0x24U
#define PM_PASSWORD 0x5a000000U
#define PM_CONTROL_MASK GENMASK(23, 0)
#define PM_RESET_MODE_MASK GENMASK(5, 4)
#define PM_FULL_RESET BIT(5)
#define PM_RESET_DELAY_TICKS 10U

#ifdef CONFIG_SOC_BCM2711
static mm_reg_t pm_base;

static int map_power_controller(void)
{
	device_map(&pm_base, DT_REG_ADDR(PM_NODE), DT_REG_SIZE(PM_NODE), K_MEM_CACHE_NONE);
	return 0;
}

SYS_INIT(map_power_controller, PRE_KERNEL_1, 0);
#else
static const uintptr_t pm_base = DT_REG_ADDR(PM_NODE);
#endif

void sys_arch_reboot(int type)
{
	ARG_UNUSED(type);

	const uint32_t control = sys_read32(pm_base + PM_RSTC) & PM_CONTROL_MASK;

	sys_write32(PM_PASSWORD | PM_RESET_DELAY_TICKS, pm_base + PM_WDOG);
	sys_write32(PM_PASSWORD | (control & ~PM_RESET_MODE_MASK) | PM_FULL_RESET,
		    pm_base + PM_RSTC);
	barrier_dsync_fence_full();

	/* Keep the CPU here until the watchdog resets the SoC. */
	for (;;) {
		__asm__ volatile("wfe");
	}
}
