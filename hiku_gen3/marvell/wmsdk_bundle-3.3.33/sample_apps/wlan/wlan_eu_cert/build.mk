# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += wlan_eu_cert
wlan_eu_cert-objs-y   := src/main.c
wlan_eu_cert-cflags-y := -I$(d)/src

# Applications could also define custom board files if required using following:
#wlan_eu_cert-board-y := /path/to/boardfile
#wlan_eu_cert-linkerscrpt-y := /path/to/linkerscript
