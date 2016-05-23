# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += wlan_trpc_demo
wlan_trpc_demo-objs-y   := src/main.c
wlan_trpc_demo-cflags-y := -D APPCONFIG_DEBUG_ENABLE -I$(d)/src

# Applications could also define custom board files if required using following:
#wlan_trpc_demo-board-y := /path/to/boardfile
#wlan_trpc_demo-linkerscrpt-y := /path/to/linkerscript
