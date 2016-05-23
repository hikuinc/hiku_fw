# Copyright 2008-2015 Marvell International Ltd.
# All Rights Reserved.

OS := $(shell uname)
ifneq ($(OS), Darwin)
STRIP = -s
endif

TARGET ?= axf2firmware
CC ?= gcc

all: $(TARGET)

$(TARGET): $(SDK_PATH)/sdk/src/core/util/crc/soft_crc32.c axf2firmware.c
	@echo " [cc] $^"
	$(AT)$(CC) -O2 -Wall $(STRIP) -I$(SDK_PATH)/sdk/src/incl/sdk -o $@ $^

clean:
	$(AT)rm -f $(TARGET) *.o
