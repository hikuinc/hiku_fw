# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += wlan_mac
wlan_mac-objs-y   := src/main.c
wlan_mac-cflags-y := -I$(d)/src

# Applications could also define custom board files if required using following:
#wlan_mac-board-y := /path/to/boardfile
#wlan_mac-linkerscrpt-y := /path/to/linkerscript
