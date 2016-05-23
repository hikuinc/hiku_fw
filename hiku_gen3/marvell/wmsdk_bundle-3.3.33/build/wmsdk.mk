# Copyright (C) 2008-2016, Marvell International Ltd.
# All Rights Reserved.
#
# Description:
# ------------
# This file, wmsdk.mk contains wmsdk specific default variables assignment.
#
#  	default defconfig
#  	default directory which contains defconfig's


# This variable holds path to example code which
# illustrates features exposed by SDK
b-examples-path-y := sample_apps

include build/sdk_common.mk

#################################################
# BOARD_FILE handling

ifdef BOARD_FILE
  override BOARD_FILE := $(call b-abspath,$(BOARD_FILE))
endif

BOARD_FILE ?= sdk/src/boards/$(BOARD).c
#################################################

######### SDK Version
SDK_VERSION_INTERNAL ?= 3.3
SDK_VERSION :=\"$(SDK_VERSION_INTERNAL)$(EXTRA_SDK_VERSION)\"
global-cflags-y += -DSDK_VERSION=$(SDK_VERSION)

# Default defconfig
b-defconfig-y := mw300_defconfig
# Path for searching SDK configuration files
b-sdkconfig-dir-y := sdk/config

include build/config.mk

