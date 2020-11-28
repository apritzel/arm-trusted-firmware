/*
 * Copyright (c) 2017-2020, ARM Limited. All rights reserved.
 * Copyright (c) 2018, Icenowy Zheng <icenowy@aosc.io>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <string.h>

#include <arch_helpers.h>
#include <common/debug.h>
#include <drivers/allwinner/axp.h>
#include <drivers/delay_timer.h>
#include <drivers/mentor/mi2cv.h>
#include <lib/mmio.h>

#include <sunxi_cpucfg.h>
#include <sunxi_def.h>
#include <sunxi_mmap.h>
#include <sunxi_private.h>

#define AXP305_ADDR	0x36

static enum pmic_type {
	UNKNOWN,
	AXP305,
} pmic;

int axp_read(uint8_t reg)
{
	uint8_t val;
	int ret;

	ret = i2c_write(AXP305_ADDR, 0, 0, &reg, 1);
	if (ret == 0)
		ret = i2c_read(AXP305_ADDR, 0, 0, &val, 1);
	if (ret) {
		ERROR("PMIC: Cannot read AXP305 register %02x\n", reg);
		return ret;
	}

	return val;
}

int axp_write(uint8_t reg, uint8_t val)
{
	int ret;

	ret = i2c_write(AXP305_ADDR, reg, 1, &val, 1);
	if (ret)
		ERROR("PMIC: Cannot write AXP305 register %02x\n", reg);

	return ret;
}

static int axp305_probe(void)
{
	int ret;

	/* Switch the AXP305 to master/single-PMIC mode. */
	ret = axp_write(0xff, 0x0);
	if (ret)
		return ret;

	ret = axp_check_id();
	if (ret)
		return ret;

	return 0;
}

int sunxi_pmic_setup(uint16_t socid, const void *fdt)
{
	int ret;

	INFO("PMIC: Probing AXP305 on I2C\n");

	ret = sunxi_init_platform_r_twi(SUNXI_SOC_H616, false);
	if (ret)
		return ret;

	/* initialise mi2cv driver */
	i2c_init((void *)SUNXI_R_I2C_BASE);

	ret = axp305_probe();
	if (ret)
		return ret;

	pmic = AXP305;
	axp_setup_regulators(fdt);

	return 0;
}

void sunxi_power_down(void)
{
	switch (pmic) {
	case AXP305:
		/* Re-initialise after rich OS might have used it. */
		sunxi_init_platform_r_twi(SUNXI_SOC_H616, false);
		/* initialise mi2cv driver */
		i2c_init((void *)SUNXI_R_I2C_BASE);
		axp_power_off();
		break;
	default:
		break;
	}
}

void sunxi_cpu_power_off_self(void)
{
	u_register_t mpidr = read_mpidr();
	unsigned int cluster = MPIDR_AFFLVL1_VAL(mpidr);
	unsigned int core    = MPIDR_AFFLVL0_VAL(mpidr);

	/*
	 * According to the Cortex-A53 TRM the core needs to be in WFI state
	 * before the reset line can be asserted, and the power be removed.
	 * This sequence can't be observed if we have only this very core
	 * to perform it.
	 * So we do as good as we can, we remove the power, then go into
	 * WFI (should we survive this).
	 * The CPU_ON sequence does a proper reset from another core, before
	 * handing this core back to the caller, so that should be fine.
	 */
	mmio_write_32(SUNXI_CPU_POWER_CLAMP_REG(cluster, core), 0xffU);
	while (1)
		wfi();
}
