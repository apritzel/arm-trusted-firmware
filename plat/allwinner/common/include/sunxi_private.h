/*
 * Copyright (c) 2017-2019, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SUNXI_PRIVATE_H
#define SUNXI_PRIVATE_H

#include <lib/psci/psci.h>

void sunxi_configure_mmu_el3(int flags);

void sunxi_cpu_on(u_register_t mpidr);
void sunxi_cpu_off(u_register_t mpidr);
void sunxi_disable_secondary_cpus(u_register_t primary_mpidr);
void sunxi_power_down(void);

int sunxi_pmic_setup(uint16_t socid, const void *fdt);
void sunxi_security_setup(void);

uint16_t sunxi_read_soc_id(void);
void sunxi_set_gpio_out(char port, int pin, bool level_high);
int sunxi_init_platform_r_twi(uint16_t socid, bool use_rsb);
void sunxi_execute_arisc_code(uint32_t *code, size_t size, uint16_t param);

extern const plat_psci_ops_t sunxi_native_psci_ops;
extern const plat_psci_ops_t sunxi_scpi_psci_ops;

#endif /* SUNXI_PRIVATE_H */
