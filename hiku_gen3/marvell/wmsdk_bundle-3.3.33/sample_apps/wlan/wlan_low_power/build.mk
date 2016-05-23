# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += wlan_low_power
wlan_low_power-objs-y   := src/main.c
wlan_low_power-cflags-y := -I$(d)/src

# Applications could also define custom board files if required using following:
#wlan_low_power-board-y := /path/to/boardfile
#wlan_low_power-linkerscrpt-y := /path/to/linkerscript
