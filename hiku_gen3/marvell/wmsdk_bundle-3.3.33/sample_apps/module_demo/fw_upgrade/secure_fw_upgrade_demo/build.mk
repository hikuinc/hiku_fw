# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

include $(d)/config.mk

exec-y += secure_fw_upgrade_demo
secure_fw_upgrade_demo-objs-y := src/main.c
secure_fw_upgrade_demo-cflags-y                       := -I$(d)/src -D APPCONFIG_DEBUG_ENABLE=1
secure_fw_upgrade_demo-cflags-$(FWUPG_DEMO_ED_CHACHA) += -DAPPCONFIG_FWUPG_ED_CHACHA
secure_fw_upgrade_demo-cflags-$(FWUPG_DEMO_RSA_AES)   += -DAPPCONFIG_FWUPG_RSA_AES

# Applications could also define custom board files if required using following:
#secure_fw_upgrade_demo-board-y := /path/to/boardfile
#secure_fw_upgrade_demo-linkerscrpt-y := /path/to/linkerscript
