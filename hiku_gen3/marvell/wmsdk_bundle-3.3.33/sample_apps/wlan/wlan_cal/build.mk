# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += wlan_cal
wlan_cal-objs-y   := src/main.c
wlan_cal-cflags-y := -I$(d)/src

# Applications could also define custom board files if required using following:
#wlan_cal-board-y := /path/to/boardfile
#wlan_cal-linkerscrpt-y := /path/to/linkerscript
