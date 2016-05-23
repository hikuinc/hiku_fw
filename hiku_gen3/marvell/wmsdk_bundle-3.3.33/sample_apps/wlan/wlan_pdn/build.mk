# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += wlan_pdn
wlan_pdn-objs-y   := src/main.c
wlan_pdn-cflags-y := -I$(d)/src

# Applications could also define custom board files if required using following:
#wlan_pdn-board-y := /path/to/boardfile
#wlan_pdn-linkerscrpt-y := /path/to/linkerscript
