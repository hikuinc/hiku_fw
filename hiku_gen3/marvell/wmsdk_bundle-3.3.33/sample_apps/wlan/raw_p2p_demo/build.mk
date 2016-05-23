# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += raw_p2p_demo

raw_p2p_demo-objs-y   := src/main.c src/p2p_helper.c src/power_mgr_helper.c
raw_p2p_demo-cflags-y := -I$(d)/src -D APPCONFIG_DEBUG_ENABLE=1

ifeq (y, $(P2P_AUTOGO))
 raw_p2p_demo-cflags-y += -DP2P_AUTOGO=1
else
 raw_p2p_demo-cflags-y += -DP2P_AUTOGO=0
endif

raw_p2p_demo-ftfs-y := raw_p2p_demo.ftfs
raw_p2p_demo-ftfs-dir-y     := $(d)/www
raw_p2p_demo-ftfs-api-y := 100

# Applications could also define custom board files if required using following:
#raw_p2p_demo-board-y := /path/to/boardfile
#raw_p2p_demo raw_p2p_demo-cflags-y += -DP2P_AUTOGO=1 raw_p2p_demo-cflags-y += -DP2P_AUTOGO=0-linkerscrpt-y := /path/to/linkerscript
