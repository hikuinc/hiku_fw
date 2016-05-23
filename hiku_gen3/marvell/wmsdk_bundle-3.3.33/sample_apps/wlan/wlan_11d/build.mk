# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += wlan_11d
wlan_11d-objs-y   := src/main.c
wlan_11d-cflags-y := -I$(d)/src

# Applications could also define custom board files if required using following:
#wlan_11d-board-y := /path/to/boardfile
#wlan_11d-linkerscrpt-y := /path/to/linkerscript
