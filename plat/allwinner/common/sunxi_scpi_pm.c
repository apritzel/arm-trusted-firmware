/*
 * Copyright (c) 2017-2020, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <platform_def.h>

#include <arch_helpers.h>
#include <common/debug.h>
#include <drivers/arm/css/css_scpi.h>
#include <drivers/arm/gicv2.h>
#include <lib/psci/psci.h>

#include <sunxi_mmap.h>
#include <sunxi_private.h>

#define CPU_PWR_LVL			MPIDR_AFFLVL0
#define CLUSTER_PWR_LVL			MPIDR_AFFLVL1
#define SYSTEM_PWR_LVL			MPIDR_AFFLVL2

#define CPU_PWR_STATE(state) \
	((state)->pwr_domain_state[CPU_PWR_LVL])
#define CLUSTER_PWR_STATE(state) \
	((state)->pwr_domain_state[CLUSTER_PWR_LVL])
#define SYSTEM_PWR_STATE(state) \
	((state)->pwr_domain_state[SYSTEM_PWR_LVL])

static inline scpi_power_state_t scpi_map_state(plat_local_state_t psci_state)
{
	if (is_local_state_run(psci_state))
		return scpi_power_on;
	if (is_local_state_retn(psci_state))
		return scpi_power_retention;
	return scpi_power_off;
}

static void sunxi_cpu_standby(plat_local_state_t cpu_state)
{
	u_register_t scr = read_scr_el3();

	assert(is_local_state_retn(cpu_state));

	write_scr_el3(scr | SCR_IRQ_BIT);
	wfi();
	write_scr_el3(scr);
}

static int sunxi_pwr_domain_on(u_register_t mpidr)
{
	scpi_set_css_power_state(mpidr,
				 scpi_power_on,
				 scpi_power_on,
				 scpi_power_on);

	return PSCI_E_SUCCESS;
}

static void sunxi_pwr_domain_off(const psci_power_state_t *target_state)
{
	plat_local_state_t cpu_pwr_state     = CPU_PWR_STATE(target_state);
	plat_local_state_t cluster_pwr_state = CLUSTER_PWR_STATE(target_state);
	plat_local_state_t system_pwr_state  = SYSTEM_PWR_STATE(target_state);

	if (is_local_state_off(cpu_pwr_state))
		gicv2_cpuif_disable();

	scpi_set_css_power_state(read_mpidr(),
				 scpi_map_state(cpu_pwr_state),
				 scpi_map_state(cluster_pwr_state),
				 scpi_map_state(system_pwr_state));
}

static void sunxi_pwr_domain_on_finish(const psci_power_state_t *target_state)
{
	if (is_local_state_off(SYSTEM_PWR_STATE(target_state)))
		gicv2_distif_init();
	if (is_local_state_off(CPU_PWR_STATE(target_state))) {
		gicv2_pcpu_distif_init();
		gicv2_cpuif_enable();
	}
}

static void __dead2 sunxi_system_off(void)
{
	uint32_t ret;

	gicv2_cpuif_disable();

	/* Send the power down request to the SCP */
	ret = scpi_sys_power_state(scpi_system_shutdown);
	if (ret == SCP_OK)
		wfi();

	ERROR("PSCI: SCPI %s failed: %d\n", "shutdown", ret);
	panic();
}

static void __dead2 sunxi_system_reset(void)
{
	uint32_t ret;

	gicv2_cpuif_disable();

	/* Send the system reset request to the SCP */
	ret = scpi_sys_power_state(scpi_system_reboot);
	if (ret == SCP_OK)
		wfi();

	ERROR("PSCI: SCPI %s failed: %d\n", "reboot", ret);
	panic();
}

static int sunxi_validate_power_state(unsigned int power_state,
				      psci_power_state_t *req_state)
{
	unsigned int power_level = psci_get_pstate_pwrlvl(power_state);
	unsigned int type = psci_get_pstate_type(power_state);

	if (power_level > PLAT_MAX_PWR_LVL)
		return PSCI_E_INVALID_PARAMS;

	if (type == PSTATE_TYPE_STANDBY) {
		/* Only one retention power state is supported. */
		if (psci_get_pstate_id(power_state) > 0)
			return PSCI_E_INVALID_PARAMS;
		/* The SoC cannot be suspended without losing state */
		if (power_level == SYSTEM_PWR_LVL)
			return PSCI_E_INVALID_PARAMS;
		for (unsigned int i = 0; i <= power_level; ++i)
			req_state->pwr_domain_state[i] = PLAT_MAX_RET_STATE;
	} else {
		/* Only one off power state is supported. */
		if (psci_get_pstate_id(power_state) > 0)
			return PSCI_E_INVALID_PARAMS;
		for (unsigned int i = 0; i <= power_level; ++i)
			req_state->pwr_domain_state[i] = PLAT_MAX_OFF_STATE;
	}
	/* Higher power domain levels should all remain running */
	for (unsigned int i = power_level + 1; i <= PLAT_MAX_PWR_LVL; ++i)
		req_state->pwr_domain_state[i] = PSCI_LOCAL_STATE_RUN;

	return PSCI_E_SUCCESS;
}

static int sunxi_validate_ns_entrypoint(uintptr_t ns_entrypoint)
{
	/* The non-secure entry point must be in DRAM */
	if (ns_entrypoint < SUNXI_DRAM_BASE)
		return PSCI_E_INVALID_ADDRESS;

	return PSCI_E_SUCCESS;
}

static void sunxi_get_sys_suspend_power_state(psci_power_state_t *req_state)
{
	for (unsigned int i = 0; i <= PLAT_MAX_PWR_LVL; ++i)
		req_state->pwr_domain_state[i] = PLAT_MAX_OFF_STATE;
}

const plat_psci_ops_t sunxi_scpi_psci_ops = {
	.cpu_standby			= sunxi_cpu_standby,
	.pwr_domain_on			= sunxi_pwr_domain_on,
	.pwr_domain_off			= sunxi_pwr_domain_off,
	.pwr_domain_suspend		= sunxi_pwr_domain_off,
	.pwr_domain_on_finish		= sunxi_pwr_domain_on_finish,
	.pwr_domain_suspend_finish	= sunxi_pwr_domain_on_finish,
	.system_off			= sunxi_system_off,
	.system_reset			= sunxi_system_reset,
	.validate_power_state		= sunxi_validate_power_state,
	.validate_ns_entrypoint		= sunxi_validate_ns_entrypoint,
	.get_sys_suspend_power_state	= sunxi_get_sys_suspend_power_state,
};
