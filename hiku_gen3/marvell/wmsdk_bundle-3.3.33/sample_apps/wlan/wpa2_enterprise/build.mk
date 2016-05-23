# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += wpa2_enterprise
wpa2_enterprise-objs-y := src/wpa2_network.c src/main.c src/power_mgr_helper.c
wpa2_enterprise-objs-y += src/mdns_helper.c

wpa2_enterprise-cflags-y := -DAPPCONFIG_MDNS_ENABLE -I$(d)/src
# Enable for debug logs
#wpa2_enterprise-cflags-y += -D APPCONFIG_DEBUG_ENABLE

# Applications could also define custom board files if required using following:
#wpa2_enterprise-board-y := /path/to/boardfile
#wpa2_enterprise wpa2_enterprise-objs-y += src/mdns_helper.c #wpa2_enterprise-cflags-y += -D APPCONFIG_DEBUG_ENABLE-linkerscrpt-y := /path/to/linkerscript
