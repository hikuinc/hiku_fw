# Copyright (C) 2016 Marvell International Ltd.
# All Rights Reserved.

# Makefile.inc: Some variable definitions to build CMSIS DSPLib

# Set root directory and import common variables
export DSPLROOT := $(d)
INCDIR := $(DSPLROOT)/Include

# FLOAT_ABI can be soft, softfp or hard. Refer to GCC manual for details
# Applicable to CM4 only
FLOAT_ABI = softfp

# Common flags for building stuff
global-cflags-y += -I$(INCDIR) -fno-strict-aliasing -DARM_MATH_MATRIX_CHECK -DARM_MATH_ROUNDING -DUNALIGNED_SUPPORT_DISABLE

global-cflags-$(CONFIG_CPU_MW300) += -mcpu=cortex-m4 -DARM_MATH_CM4

global-cflags-$(CONFIG_CPU_MC200) += -DARM_MATH_CM3 -mcpu=cortex-m3


ifneq ($(FLOAT_ABI), soft)
global-cflags-$(CONFIG_CPU_MW300) += -mfpu=fpv4-sp-d16 -mfloat-abi=$(FLOAT_ABI) -ffp-contract=off	-D__FPU_PRESENT=1
endif

-include $(d)/build.cmsis.mk

