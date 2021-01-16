/*
 * Copyright (c) 2017-2020, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>

#include <common/debug.h>
#include <drivers/arm/css/css_scpi.h>
#include <lib/mmio.h>
#include <lib/psci/psci.h>

#include <sunxi_cpucfg.h>
#include <sunxi_private.h>

/*
 * The addresses for the SCP exception vectors are defined in the or1k
 * architecture specification.
 */
#define OR1K_VEC_FIRST			0x01
#define OR1K_VEC_LAST			0x0e
#define OR1K_VEC_ADDR(n)		(0x100 * (n))

/*
 * This magic value is the little-endian representation of the or1k
 * instruction "l.mfspr r2, r0, 0x12", which is guaranteed to be the
 * first instruction in the SCP firmware.
 */
#define SCP_FIRMWARE_MAGIC		0xb4400012

int plat_setup_psci_ops(uintptr_t sec_entrypoint,
			const plat_psci_ops_t **psci_ops)
{
	bool scpi_available = false;

	/* Program all CPU entry points. */
	for (unsigned int cpu = 0; cpu < PLATFORM_CORE_COUNT; ++cpu) {
		mmio_write_32(SUNXI_CPUCFG_RVBAR_LO_REG(cpu),
			      sec_entrypoint & 0xffffffff);
		mmio_write_32(SUNXI_CPUCFG_RVBAR_HI_REG(cpu),
			      sec_entrypoint >> 32);
	}

	/* Check for a valid SCP firmware, and boot the SCP if found. */
	if (mmio_read_32(SUNXI_SCP_BASE) == SCP_FIRMWARE_MAGIC) {
		/* Program SCP exception vectors to the firmware entrypoint. */
		for (unsigned int i = OR1K_VEC_FIRST; i <= OR1K_VEC_LAST; ++i) {
			uint32_t vector = SUNXI_SRAM_A2_BASE + OR1K_VEC_ADDR(i);
			uint32_t offset = SUNXI_SCP_BASE - vector;

			mmio_write_32(vector, offset >> 2);
			clean_dcache_range(vector, sizeof(uint32_t));
		}
		/* Take the SCP out of reset. */
		mmio_setbits_32(SUNXI_R_CPUCFG_BASE, BIT(0));
		/* Wait for the SCP firmware to boot. */
		if (scpi_wait_ready() == 0)
			scpi_available = true;
	}

	NOTICE("PSCI: System suspend is %s\n",
	       scpi_available ? "available via SCPI" : "unavailable");

	*psci_ops = scpi_available ? &sunxi_scpi_psci_ops
				   : &sunxi_native_psci_ops;

	return 0;
}
