# Copyright (C) 2015, Marvell International Ltd.
# All Rights Reserved.

CC?=gcc
AR?=ar

export CC
export AR
export CFLAGS

export COMMON_DIR=$(CURDIR)/../../../src/core/util

OS ?= $(shell uname)

ifneq ($(OS), Linux)
ifneq ($(OS), Darwin)
TARGET = secureboot.exe
endif
endif
TARGET ?= secureboot

SRCS = main.c boot2_generator.c fw_generator.c parse_config.c utils.c otp.c \
	encrypt.c signing.c hash.c

SRCS += $(COMMON_DIR)/wmtlv.c $(COMMON_DIR)/crc/soft_crc16.c \
	$(COMMON_DIR)/crc/soft_crc32.c

CFLAGS += -O2 -DCONFIG_CPU_MW300 -Wall -fdata-sections -ffunction-sections
CFLAGS += -I . -I $(CURDIR)/../../../src/incl/sdk/

LDFLAGS = -lcrypto

# OpenSSL is deperecated in Mac 10.7 and later
# Linker flags -s and --gc-sections are also not supported on Mac
ifeq ($(OS), Darwin)
CFLAGS += -Wno-deprecated-declarations
OPENSSL_INSTALL_DIR ?= /opt/local
else
LDFLAGS += -s -Wl,--gc-sections
endif

ifneq ($(OPENSSL_INSTALL_DIR),)
CFLAGS += -I $(OPENSSL_INSTALL_DIR)/include
LDFLAGS += -L $(OPENSSL_INSTALL_DIR)/lib
OSSL_BIN := $(shell (which $(OPENSSL_INSTALL_DIR)/bin/openssl 2> /dev/null))
else
OSSL_BIN := $(shell (which openssl 2> /dev/null))
endif

# This script assigns to OSSL_VER, 0 if required version, 1 if no openssl or 2 if older version
ifneq ($(OSSL_BIN),)
OSSL_VER := $(word 2,$(shell $(OSSL_BIN) version -v))
ifeq ($(shell expr $(OSSL_VER) \>= 1.0.1),1)
OSSL_VER := 0
else
OSSL_VER := 2
endif
else
OSSL_VER := 1
endif

define ERROR_MSG_1

*** OpenSSL is not installed on this machine
*** Please refer to README for latest OpenSSL installation instructions
*** Skipping $(TARGET) build...

endef
define ERROR_MSG_2

*** OpenSSL Version is older than 1.0.1
*** Please refer to README for latest OpenSSL installation instructions
*** Skipping $(TARGET) build...

endef
define check_version
  $(if $(filter 0,$(OSSL_VER)),,$(error $(ERROR_MSG_$(OSSL_VER))))
endef

ifneq ($(or $(findstring MINGW,$(OS)), $(findstring windows,$(OS))),)
CFLAGS += -I /include -mno-ms-bitfields
LDFLAGS += -lgdi32
else
LDFLAGS += -ldl
endif

ifneq ($(TEST_BUILD),)
CFLAGS += -DTEST_BUILD
endif

ifneq ($(NOISY),1)
AT=@
SILENT=-s
else
AT=
endif

OBJS = $(notdir $(SRCS:.c=.o))
vpath %c $(COMMON_DIR) $(COMMON_DIR)/crc

.PHONY: clean install

all: $(TARGET)

%.o: %.c
	$(call check_version)
	$(AT)echo " [cc] $^"
	$(AT)$(CC) $(CFLAGS) -c -o $@ $^

$(TARGET) : $(OBJS)
	$(call check_version)
	$(AT)echo " [ar] $@"
	$(AT)$(CC) $(OBJS) $(LIBS) $(LDFLAGS) -o $(TARGET)

install:
	$(AT)if [ -x $(TARGET) ]; then \
	echo " [install] $(TARGET)"; \
	mkdir -p $(SDK_DIR)/tools/bin/$(TOOLS_BINDIR); \
	$(COPY_CMD) -a $(TARGET) $(SDK_DIR)/tools/bin/$(TOOLS_BINDIR); \
	fi;

clean_objs:
	rm -f *.o *.a *.d $(TARGET)

clean: clean_objs
