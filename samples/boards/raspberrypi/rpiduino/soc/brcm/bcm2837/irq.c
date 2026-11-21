/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/arch/cpu.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/timer/arm_arch_timer.h>
#include <zephyr/sys/barrier.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/util.h>

#define LOCAL_BASE DT_REG_ADDR(DT_NODELABEL(local_intc))
#define CORE0_TIMER_CONTROL (LOCAL_BASE + 0x40U)
#define CORE0_MAILBOX_CONTROL (LOCAL_BASE + 0x50U)
#define CORE0_IRQ_SOURCE (LOCAL_BASE + 0x60U)
#define TIMER_ROUTE BIT(ARM_TIMER_VIRTUAL_IRQ)

static bool timer_enabled;
static bool timer_in_service;

static void update_timer_route(void)
{
	sys_write32(timer_enabled && !timer_in_service ? TIMER_ROUTE : 0U,
		    CORE0_TIMER_CONTROL);
	barrier_dsync_fence_full();
}

void z_soc_irq_init(void)
{
	sys_write32(0U, CORE0_MAILBOX_CONTROL);
	update_timer_route();
}

void z_soc_irq_enable(unsigned int irq)
{
	if (irq == ARM_TIMER_VIRTUAL_IRQ) {
		unsigned int key = arch_irq_lock();

		timer_enabled = true;
		update_timer_route();
		arch_irq_unlock(key);
	}
}

void z_soc_irq_disable(unsigned int irq)
{
	if (irq == ARM_TIMER_VIRTUAL_IRQ) {
		unsigned int key = arch_irq_lock();

		timer_enabled = false;
		update_timer_route();
		arch_irq_unlock(key);
	}
}

int z_soc_irq_is_enabled(unsigned int irq)
{
	return irq == ARM_TIMER_VIRTUAL_IRQ && timer_enabled;
}

void z_soc_irq_priority_set(unsigned int irq, unsigned int prio, unsigned int flags)
{
	/* This port routes one fixed-priority interrupt to core 0. */
	ARG_UNUSED(irq);
	ARG_UNUSED(prio);
	ARG_UNUSED(flags);
}

unsigned int z_soc_irq_get_active(void)
{
	if ((sys_read32(CORE0_IRQ_SOURCE) & TIMER_ROUTE) == 0U) {
		return CONFIG_NUM_IRQS;
	}

	/* The IRQ wrapper unmasks CPU IRQs while the level-triggered ISR runs. */
	timer_in_service = true;
	update_timer_route();
	return ARM_TIMER_VIRTUAL_IRQ;
}

void z_soc_irq_eoi(unsigned int irq)
{
	if (irq == ARM_TIMER_VIRTUAL_IRQ) {
		timer_in_service = false;
		update_timer_route();
	}
}
