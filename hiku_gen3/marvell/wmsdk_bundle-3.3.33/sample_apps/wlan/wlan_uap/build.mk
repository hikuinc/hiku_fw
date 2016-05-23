# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += wlan_uap

wlan_uap-objs-y := src/main.c
wlan_uap-cflags-y := -I$(d)/src

WLAN_UAP_MDNS_ENABLE=y
wlan_uap-objs-$(WLAN_UAP_MDNS_ENABLE)   += src/mdns_helper.c
wlan_uap-cflags-$(WLAN_UAP_MDNS_ENABLE) += -DAPPCONFIG_MDNS_ENABLE

# Enable for debugging
# wlan_uap-cflags-y += -D APPCONFIG_DEBUG_ENABLE


# Applications could also define custom board files if required using following:
#wlan_uap-board-y := /path/to/boardfile
#wlan_uap wlan_uap-objs-$(WLAN_UAP_MDNS_ENABLE) += src/mdns_helper.c wlan_uap-cflags-$(WLAN_UAP_MDNS_ENABLE) += -DAPPCONFIG_MDNS_ENABLE # wlan_uap-cflags-y += -D APPCONFIG_DEBUG_ENABLE-linkerscrpt-y := /path/to/linkerscript
