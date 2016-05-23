# Copyright (C) 2008-2016, Marvell International Ltd.
# All Rights Reserved.

CROSS_COMPILE=arm-none-eabi-

AS    := $(CROSS_COMPILE)gcc
CC    := $(CROSS_COMPILE)gcc
CPP   := $(CROSS_COMPILE)g++
LD    := $(CROSS_COMPILE)ld
AR    := $(CROSS_COMPILE)ar
OBJCOPY := $(CROSS_COMPILE)objcopy
STRIP := $(CROSS_COMPILE)strip
HOST_CC := gcc


######### Common Linker File Handling
# This can be overriden from the apps
global-linkerscript-y := build/toolchains/GNU/$(arch_name-y).ld

######### XIP Handling
ifeq ($(XIP), 1)
  global-linkerscript-y := build/toolchains/GNU/$(arch_name-y)-xip.ld
  global-linkerscript-$(CONFIG_ENABLE_MCU_PM3) := build/toolchains/GNU/$(arch_name-y)-xip-pm3.ld
  global-cflags-y += -DCONFIG_XIP_ENABLE
endif


compiler-version := $(shell $(CC) -dumpversion)
ifneq ($(compiler-version),4.9.3)
$(error " Please use: $(CC) 2015 q3 version")
endif

# This variable will be  moved to
# toolchain specific mk file later
# Fixme
disable-lto-for :=

# Compiler options
linker-opt := -Xlinker
map-opt := -Xlinker -M -Xlinker -Map -Xlinker
ld-opt := -T
start-group-opt := -Xlinker --start-group
end-group-opt := -Xlinker --end-group

# library strip options
strip-opt := --strip-debug

# Compiler environment variable
tc-env := GCC

# gcc specific extra linker flags
toolchain-linkerscript-flags-y := \
	-Xlinker --undefined -Xlinker uxTopUsedPriority \
	-nostartfiles -Xlinker --cref -Xlinker --gc-sections

# gcc specific global-cflags-y
global-cflags-y := \
	-mthumb -g -Os -fdata-sections -ffunction-sections -ffreestanding \
	-MMD -Wall -fno-strict-aliasing -fno-common

global-cflags-$(tc-cortex-m4-y) += \
	-mfloat-abi=softfp -mfpu=fpv4-sp-d16 -D__FPU_PRESENT

global-cflags-$(tc-cortex-m3-y) += -mcpu=cortex-m3
global-cflags-$(tc-cortex-m4-y) += -mcpu=cortex-m4

LDFLAGS-$(tc-cortex-m4-y) += -Xlinker --defsym=_rom_data=64
LDFLAGS-$(tc-lto-y) += -Xlinker -flto

global-cpp-cflags-y := -D_Bool=bool -std=c++1y --specs=nosys.specs
global-c-cflags-y := -fgnu89-inline
