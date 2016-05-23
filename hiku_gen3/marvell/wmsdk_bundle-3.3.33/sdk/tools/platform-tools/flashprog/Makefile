# Copyright 2008-2015 Marvell International Ltd.
# All Rights Reserved.


ifeq ($(SDK_DIR),)
$(error " Please specify SDK_DIR i.e. path to bundle For e.g: make SDK_DIR=/path/to/wmsdk_bundle-x.y.z")
endif

ifeq ($(LIBS_PATH),)
$(error " Please specify LIBS_PATH i.e. path to libs")
endif


include arm-gnu.mk

ifeq ($(NOISY),1)
AT=
else
AT=@
endif

# Tools makefile for GNU tools
include $(SDK_DIR)/.config
ARCH_NAME-$(CONFIG_CPU_MW300) := mw300
ARCH_NAME-$(CONFIG_CPU_MC200) := mc200
ARCH-$(CONFIG_CPU_MW300) := cortex-m4
ARCH-$(CONFIG_CPU_MC200) := cortex-m3

CFLAGS += -mcpu=$(ARCH-y)

CC := arm-none-eabi-gcc
AS := arm-none-eabi-as
STRIP := arm-none-eabi-strip


%.o: %.c $(SDK_DIR)/sdk/src/incl/autoconf.h
	$(AT)$(CC) $(CFLAGS) $(EXTRACFLAGS) -c $<
	@echo " [cc] $<"

%.o: %.S $(SDK_DIR)/sdk/src/incl/autoconf.h
	$(AT)$(CC) -c $<
	@echo " [cc] $<"

TARGET ?= flashprog.axf

all: $(TARGET)

OBJS += flashprog.o flash.o crc32.o firmware.o syscall.o

ifeq (y,$(CONFIG_SPI_FLASH_DRIVER))
MCU_LIB := libspiflash.a
endif

MCU_LIB += libdrv.a libpart.a libftfs.a

LIBS := $(addprefix $(SDK_DIR)/$(LIBS_PATH)/, $(MCU_LIB))
CFLAGS += \
	-I $(SDK_DIR)/sdk/src/incl/platform/os/freertos/  \
	-I $(SDK_DIR)/sdk/src/incl/sdk/drivers/$(ARCH_NAME-y)/ \
	-I $(SDK_DIR)/sdk/src/incl/sdk/drivers/$(ARCH_NAME-y)/regs/ \
	-I $(SDK_DIR)/sdk/src/incl/sdk/drivers/ \
	-I $(SDK_DIR)/sdk/src/incl/sdk/ \
	-include $(SDK_DIR)/sdk/src/incl/autoconf.h

$(TARGET): $(OBJS)
	@echo " [make] $@"
	@echo " [OBJS] $^"
	$(AT)$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o $@
	$(AT)$(STRIP) $@

clean:
	@rm -f *.o $(TARGET)
