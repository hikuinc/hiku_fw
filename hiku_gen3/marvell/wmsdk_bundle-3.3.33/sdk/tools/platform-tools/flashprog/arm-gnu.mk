# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

# Use newlib-nano. To disable it, specify USE_NANO=
USE_NANO=--specs=nano.specs

# Use seimhosting or not
USE_SEMIHOST=--specs=rdimon.specs -lc -lc -lrdimon
USE_NOHOST=-lc -lc -lnosys

# Options for specific architecture
ARCH_FLAGS=-mthumb

# -Os -flto -ffunction-sections -fdata-sections to compile for code size
# ARM_GNU macro is added to include a workaround for sscanf with ARM tool chain
CFLAGS=$(ARCH_FLAGS) -Os -flto -ffunction-sections -fdata-sections -DARM_GNU

# Startup code
OBJS = startup_ARMCMx.o

# Link for code size
GC=-Wl,--gc-sections

LDSCRIPTS=-L. -T  mcu_flashprog.ld
LDFLAGS= $(USE_SEMIHOST) $(LDSCRIPTS) $(GC) $(USE_NANO)
