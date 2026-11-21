/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/arch/arm64/arm_mmu.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>

#define DEVICE_ATTRS (MT_DEVICE_nGnRnE | MT_P_RW_U_NA | MT_DEFAULT_SECURE_STATE)
#define DEVICE_REGION(label)                                                                       \
	MMU_REGION_FLAT_ENTRY(#label, DT_REG_ADDR(DT_NODELABEL(label)),                             \
			      DT_REG_SIZE(DT_NODELABEL(label)), DEVICE_ATTRS)

static const struct arm_mmu_region regions[] = {
	DEVICE_REGION(gpio),
	DEVICE_REGION(uart1),
	DEVICE_REGION(local_intc),
};

const struct arm_mmu_config mmu_config = {
	.num_regions = ARRAY_SIZE(regions),
	.mmu_regions = regions,
};
