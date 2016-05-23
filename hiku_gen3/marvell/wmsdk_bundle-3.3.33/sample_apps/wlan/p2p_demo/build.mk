# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += p2p_demo
p2p_demo-objs-y     := src/main.c src/power_mgr_helper.c
p2p_demo-cflags-y   := -I$(d)/src -D APPCONFIG_DEBUG_ENABLE=1

p2p_demo-ftfs-y := p2p_demo.ftfs
p2p_demo-ftfs-dir-y     := $(d)/www
p2p_demo-ftfs-api-y := 100


# Applications could also define custom board files if required using following:
#p2p_demo-board-y := /path/to/boardfile
#p2p_demo-linkerscrpt-y := /path/to/linkerscript
