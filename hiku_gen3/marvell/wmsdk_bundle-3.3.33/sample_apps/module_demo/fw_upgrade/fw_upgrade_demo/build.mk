# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

include $(d)/config.mk

exec-y += fw_upgrade_demo
fw_upgrade_demo-objs-y := src/main.c
fw_upgrade_demo-cflags-y                       := -I$(d)/src -D APPCONFIG_DEBUG_ENABLE=1
fw_upgrade_demo-cflags-$(MID_LEVEL_FW_UPGRADE) += -DAPPCONFIG_MID_LEVEL_FW_UPGRADE
fw_upgrade_demo-cflags-$(HIGH_LEVEL_FW_UPGRADE) += -DAPPCONFIG_HIGH_LEVEL_FW_UPGRADE

# Applications could also define custom board files if required using following:
#fw_upgrade_demo-board-y := /path/to/boardfile
#fw_upgrade_demo-linkerscrpt-y := /path/to/linkerscript
