# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

libs-y += libwifidriver

libwifidriver-objs-y := wifi-mem.c wifi_pwrmgr.c wifi.c wifi-uap.c
libwifidriver-objs-y += wifi-debug.c wifi-sdio.c mlan_uap_ioctl.c
libwifidriver-objs-y += mlan_11n.c mlan_11n_rxreorder.c mlan_init.c
libwifidriver-objs-y += mlan_cmdevt.c mlan_join.c mlan_cfp.c mlan_glue.c
libwifidriver-objs-y += mlan_txrx.c mlan_sta_rx.c mlan_misc.c mlan_shim.c
libwifidriver-objs-y += mlan_wmm.c mlan_11n_aggr.c mlan_sta_cmd.c
libwifidriver-objs-y += mlan_sta_cmdresp.c mlan_sta_event.c mlan_wmsdk.c
libwifidriver-objs-y += mlan_11h.c mlan_scan.c mlan_11d.c
libwifidriver-objs-y += mlan_sta_ioctl.c mlan_sdio.c mlan_uap_cmdevent.c

libwifidriver-objs-$(CONFIG_WPS2)   += wifi-wps.c
libwifidriver-objs-$(CONFIG_P2P)    += wifi_p2p.c wifi-p2p.c wifi_p2p_uap.c

libwifidriver-cflags-y              := -I$(d) -I$(d)/incl
libwifidriver-cflags-$(FIT_FOR_PM3) += -DFIT_FOR_PM3
