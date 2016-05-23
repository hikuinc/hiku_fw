# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += wlan_sniffer
wlan_sniffer-objs-y   := src/main.c
wlan_sniffer-cflags-y :=-I$(d)/src -D APPCONFIG_DEBUG_ENABLE=1
# Applications could also define custom board files if required using following:
#wlan_sniffer-board-y := /path/to/boardfile
#wlan_sniffer-linkerscrpt-y := /path/to/linkerscript
