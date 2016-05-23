# Copyright (C) 2008-2016, Marvell International Ltd.
# All Rights Reserved.

# Please refer to:
#  - build/README for quick instructions on writing a build.mk file
#  - Build System Documentation for detailed explanation of the build
#    system
-include .config
include build/wmsdk.mk

############################################
# SDK Modules
subdir-y += sdk/src/core/cli
subdir-y += sdk/src/core/wmstdio
subdir-y += sdk/src/core/healthmon
subdir-y += sdk/src/core/partition
subdir-y += sdk/src/core/diagnostics
subdir-y += sdk/src/core/overlays
subdir-y += sdk/src/core/mdev
subdir-y += sdk/src/core/pwrmgr
subdir-y += sdk/src/core/util
subdir-y += sdk/src/core/wmtime
subdir-y += sdk/src/ez_api
subdir-y += sdk/src/libc/src
subdir-y += sdk/src/middleware/rfget
subdir-y += sdk/src/middleware/json_parser
subdir-y += sdk/src/middleware/json_generator
subdir-y += sdk/src/middleware/telnetd
subdir-y += sdk/src/middleware/provisioning
subdir-y += sdk/src/middleware/sysinfo
subdir-y += sdk/src/middleware/ttcp
subdir-y += sdk/src/middleware/websockets
subdir-y += sdk/src/middleware/nw_utils
subdir-y += sdk/src/middleware/tls
subdir-y += sdk/src/middleware/cache_profile
subdir-y += sdk/src/middleware/crypto/aescrypto
subdir-y += sdk/src/middleware/crypto/ed25519
subdir-y += sdk/src/middleware/crypto/poly1305
subdir-y += sdk/src/middleware/dhcpd
subdir-y += sdk/src/middleware/ezxml
subdir-y += sdk/src/middleware/ftfs
subdir-y += sdk/src/middleware/httpc
subdir-y += sdk/src/middleware/httpd
subdir-y += sdk/src/middleware/psm
subdir-y += sdk/src/middleware/mfg
subdir-y += sdk/src/middleware/mdns
subdir-y += sdk/src/middleware/crypto
subdir-y += sdk/src/middleware/crypto/bn
subdir-y += sdk/src/middleware/crypto/chacha-20
subdir-y += sdk/src/middleware/crypto/hkdf-sha512
subdir-y += sdk/src/middleware/crypto/srp6a
subdir-y += sdk/src/middleware/crypto/curve25519
subdir-y += sdk/src/wlan/btdriver
subdir-y += sdk/src/wlan/wifidriver
subdir-y += sdk/src/wlan/wlcmgr
subdir-y += sdk/src/drivers/peripherals/led_indicator
subdir-y += sdk/src/drivers/peripherals/push_button
subdir-y += sdk/src/app_framework
subdir-y += sdk/src/platform/os/freertos
subdir-y += sdk/src/platform/net/lwip
subdir-y += sdk/external/freertos
subdir-y += sdk/external/lwip
subdir-y += sdk/external/cyassl
subdir-y += sdk/external/expat-2.1.0
subdir-y += sdk/external/arrayent
subdir-y += sdk/external/evrythng
subdir-y += sdk/external/aws_iot
subdir-y += sdk/external/libxively

############################################
# Configurable components
subdir-$(CONFIG_WPS2) += sdk/src/middleware/wps
subdir-$(CONFIG_HOST_SUPP) += sdk/src/wlan/wm_supplicant
subdir-$(CONFIG_CPU_MC200) += sdk/src/drivers/mc200
subdir-$(CONFIG_CPU_MW300) += sdk/src/drivers/mw300
subdir-$(CONFIG_XFLASH_DRIVER)    += sdk/src/drivers/peripherals/xflash
subdir-$(CONFIG_SPI_FLASH_DRIVER) += sdk/src/drivers/peripherals/spi_flash
subdir-$(CONFIG_CMSIS_DSPLIB) += sdk/external/cmsis
subdir-$(CONFIG_USB_DRIVER) += sdk/external/usb

ifeq ($(CONFIG_ENABLE_ROM_LIBS), )
subdir-$(CONFIG_XZ_DECOMPRESSION) += sdk/src/middleware/xz
endif

############################################
# USB Stack handling
ifdef USB_HOST_PATH
  override USB_HOST_PATH := $(call b-abspath,$(USB_HOST_PATH))
  subdir-y += $(USB_HOST_PATH)
endif

############################################
# BLE Stack handling

ifdef BLE_STACK_PATH
  override BLE_STACK_PATH := $(call b-abspath,$(BLE_STACK_PATH))
  subdir-$(CONFIG_CPU_MC200) += $(BLE_STACK_PATH)
endif

############################################
# APP management

ifndef APP
  subdir-y += sample_apps/sample_apps.mk
else
  override APP := $(call b-abspath,$(APP))
  ifeq ($(wildcard $(APP)),)
    $(error "Please specify a valid APP path")
  endif
  subdir-y += $(APP)
endif

############################################
# Boot2
subdir-y += boot2
############################################
# Tools
subdir-y += sdk/tools/host-tools/psm-create-part
subdir-y += sdk/tools/host-tools/uartboot
subdir-y += sdk/tools/host-tools/fw_generator
subdir-y += sdk/tools/platform-tools/flashprog

############################################
# Documentation
subdir-y += docs
############################################

include build/wmsdk_rules.mk

############################################
ifneq ($(NOISY),1)
silent := -s
endif

############################################
-include prebuild.mk
############################################

### And now the targets
.PHONY: all build do_the_build post-build-1 pre-build-1 build-help help clean

# all: The build dependencies are:
#                all -> post-build-1 -> build -> pre-build-1
# This allows developers to attach their pre/post scripts by adding
# dependencies to pre-build-1 and post-build-1
all: post-build-1
post-build-1: do_the_build
do_the_build: pre-build-1
	$(AT)$(MAKE) $(silent) build

# The entire build system is dependency driven. By default, build
# all the apps and boot2. This will build the other dependencies as
# required
build: $(b-exec-apps-y) boot2

clean: boot2.clean

############################################
help: build-help

build-help:
	@echo ""
	@echo "Compile SDK and sample applications"
	@echo "-----------------------------------"
	@echo "   $$ make [BOARD=<board_name>]"
	@echo "   Supported boards can be found in sdk/src/boards/".
	@echo "   Default board is mw300_rd"
	@echo ""
	@echo "Build XIP enabled applications for 88MW30x"
	@echo "-------------------------------------"
	@echo "   $$ make [BOARD=<board_name>] XIP=1"
	@echo "   Note: With XIP ramload will not be functional for AXF image/s"
	@echo ""

############################################

