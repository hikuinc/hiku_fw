# Copyright 2008-2015 Marvell International Ltd.
# All Rights Reserved.

OS := $(shell uname)

CC ?= gcc

ifeq ($(SDK_DIR),)
$(error " Please specify SDK_DIR i.e. path to bundle For e.g: make SDK_DIR=/path/to/wmsdk_bundle-x.y.z")
endif

PSM_V2_DIR := ../../../src/middleware/psm
CRC_DIR := ../../../src/core/util/crc

ifneq ($(findstring MINGW, ${OS}), )
# no-ms-bitfields is required due to a bug in mingw
# gcc. Check-out: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=52991
CFLAGS := -DBUILD_MINGW -mno-ms-bitfields
endif

PSM_V2_SRCS := $(PSM_V2_DIR)/psm-v2.c
CRC_SRCS := $(CRC_DIR)/soft_crc32.c
UTILITY_SRCS := psm-create.c

UTILITY_OBJS := $(patsubst %.c,%.o,$(UTILITY_SRCS))
PSM_V2_OBJS := $(notdir $(patsubst %.c,%.o,$(PSM_V2_SRCS)))
CRC_OBJS := $(notdir $(patsubst %.c,%.o,$(CRC_SRCS)))

TARGET ?= psm-create-part
CFLAGS += -Wall -I. -g -O3 -I. -I$(SDK_DIR)/sdk/src/incl -I$(SDK_DIR)/sdk/src/incl/sdk \
		-I$(PSM_V2_DIR) -include $(SDK_DIR)/sdk/src/incl/autoconf.h \
		-DTRUNCATE -D__PSM_UTIL__ -Dwmprintf=printf
LDFLAGS := -lpthread

ifneq ($(SECURE_BOOT),)
CFLAGS += -DCONFIG_SECURE_PSM
endif


ifeq ($(OS), Darwin)
CFLAGS += -Wno-deprecated-declarations
endif


ifneq ($(findstring MINGW, ${OS}), )
LDFLAGS += -L /usr/lib
endif

LDFLAGS +=  -lm -lcrypto -lssl

$(TARGET): clean $(UTILITY_OBJS) $(PSM_V2_OBJS) $(CRC_OBJS)
	@echo " [$(CC)] $@"
	@$(CC)  $(CFLAGS) -o $(TARGET) $(UTILITY_OBJS) $(PSM_V2_OBJS) $(CRC_OBJS) $(LDFLAGS)

$(PSM_V2_OBJS): $(PSM_V2_SRCS)
	@$(CC) -c $(CFLAGS) -o $(notdir $@) $<

$(CRC_OBJS): $(CRC_SRCS)
	@$(CC) -c $(CFLAGS) -o $(notdir $@) $<

%.o: %.c
	@$(CC) -c $(CFLAGS) -o $(notdir $@) $<

clean:
	@rm -f *.o $(TARGET) $(UTILITY_OBJS) $(PSM_V2_OBJS) $(CRC_OBJS)

