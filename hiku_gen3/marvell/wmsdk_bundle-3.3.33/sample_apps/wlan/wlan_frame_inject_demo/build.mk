# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += wlan_frame_inject_demo
wlan_frame_inject_demo-objs-y   := src/main.c
wlan_frame_inject_demo-cflags-y := -D APPCONFIG_DEBUG_ENABLE -I$(d)/src

# Applications could also define custom board files if required using following:
#wlan_frame_inject_demo-board-y := /path/to/boardfile
#wlan_frame_inject_demo-linkerscrpt-y := /path/to/linkerscript
