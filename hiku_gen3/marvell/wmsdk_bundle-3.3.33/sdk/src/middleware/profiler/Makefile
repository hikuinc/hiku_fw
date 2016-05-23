#                Copyright 2008-2014, Marvell International Ltd.
# This code contains confidential information of Marvell Semiconductor, Inc.
# No rights are granted herein under any patent, mask work right or copyright
# of Marvell or any third party.
# Marvell reserves the right at its sole discretion to request that this code
# be immediately returned to Marvell. This code is provided "as is".
# Marvell makes no warranties, express, implied or otherwise, regarding its
# accuracy, completeness or performance.
#
# Makefile to build the sysinfo utility for reporting thread info
#

include $(SDK_PATH)/build/env/common.mak

DST_NAME = libprofiler.a
SRCS =  profiler.c profiler_cli.c

# Stub function should not be added to __gnu_mcount_nc itself.
profiler.o: CFLAGS := $(filter-out -pg,$(CFLAGS))

all:  lib

include $(SDK_PATH)/build/env/rules.mak
