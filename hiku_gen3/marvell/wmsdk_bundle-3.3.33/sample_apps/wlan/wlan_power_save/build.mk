# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += wlan_power_save
wlan_power_save-objs-y   := src/main.c
wlan_power_save-cflags-y := -I$(d)/src

# Applications could also define custom board files if required using following:
#wlan_power_save-board-y := /path/to/boardfile
#wlan_power_save-linkerscrpt-y := /path/to/linkerscript
