# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += fcc_app
fcc_app-objs-y         := src/main.c
fcc_app-cflags-y       := -I$(d)/src/ -D APPCONFIG_DEBUG_ENABLE=1
fcc_app-cflags-$(CONFIG_BT_SUPPORT) += -DAPPCONFIG_BT_SUPPORT
# Applications could also define custom board files if required using following:
#fcc_app-board-y := /path/to/boardfile
#fcc_app fcc_app-cflags-$(CONFIG_BT_SUPPORT) += -DAPPCONFIG_BT_SUPPORT-linkerscrpt-y := /path/to/linkerscript
