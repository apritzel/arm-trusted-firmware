#
# Copyright (c) 2017-2020, ARM Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# The differences between the platforms are covered by the include files.
include plat/allwinner/common/allwinner-common.mk

BL31_SOURCES		+=	drivers/allwinner/axp/axp805.c		\
				drivers/mentor/i2c/mi2cv.c		\
				common/fdt_fixup.c

$(eval $(call add_define,SUNXI_RUN_IN_DRAM))
