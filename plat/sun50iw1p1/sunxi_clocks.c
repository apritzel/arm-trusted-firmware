/*
 * Copyright (c) 2016, ARM Limited and Contributors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <debug.h>
#include <mmio.h>
#include <ccmu.h>
#include "sunxi_private.h"

int sunxi_setup_clocks(uint16_t socid)
{
	uint32_t reg;

	/* Avoid reprogramming PERIPH0 if not necessary */
	reg = mmio_read_32(CCMU_PLL_PERIPH0_CTRL_REG);
	if ((reg & 0x0fffffff) != 0x41811)		/* is not at 600 MHz? */
		mmio_write_32(CCMU_PLL_PERIPH0_CTRL_REG, 0x80041811);

	/* Check initial CPU frequency: */
	reg = mmio_read_32(CCMU_PLL_CPUX_CTRL_REG);

	if ((reg & 0x0fffffff) != 0x1010) {		/* if not at 816 MHz: */
		/* switch CPU to 24 MHz source for changing PLL1 */
		mmio_write_32(CCMU_CPUX_AXI_CFG_REG,  0x00010000);
		udelay(10);

		/* Set to 816 MHz */
		mmio_write_32(CCMU_PLL_CPUX_CTRL_REG, 0x80001010);
		udelay(10);
	}

	/* switch CPU to PLL1 source, AXI = CPU/3, APB = CPU/4 */
	mmio_write_32(CCMU_CPUX_AXI_CFG_REG,  0x00020302);
	udelay(10);

	/* AHB1 = PERIPH0 / (3 * 1) = 200MHz, APB1 = AHB1 / 2 */
	mmio_write_32(CCMU_AHB1_APB1_CFG_REG, 0x00003180);
	mmio_write_32(CCMU_APB2_CFG_GREG,     0x01000000); /* APB2 =>  24 MHz */
	mmio_write_32(CCMU_AHB2_CFG_GREG,     0x00000001); /* AHB2 => 300 MHz */

	return 0;
}
