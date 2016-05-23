# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

libs-y += libwlcmgr
libwlcmgr-objs-y := wlan.c wlan_smc.c fw_heartbeat.c
libwlcmgr-objs-y += wlan_tests.c wlan_basic_cli.c iw.c uaputl.c


WLAN_DRV_VERSION_INTERNAL=1.0
WLAN_DRV_VERSION=\"$(WLAN_DRV_VERSION_INTERNAL)\"
libwlcmgr-cflags-y += -DWLAN_DRV_VERSION=$(WLAN_DRV_VERSION)
