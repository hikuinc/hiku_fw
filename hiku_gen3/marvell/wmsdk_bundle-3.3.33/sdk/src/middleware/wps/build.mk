# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

libs-y += libwps
libwps-objs-y := wps_mem.c wps_state.c wps_eapol.c
libwps-objs-y += wps_msg.c wps_start.c wps_os.c
libwps-objs-y += wps_l2.c wps_util.c wps_main.c wps_wlan.c

libwps-objs-$(CONFIG_P2P)       += wfd_main.c wfd_action.c

libwps-objs-$(CONFIG_WPS_DEBUG) := -DSTDOUT_DEBUG -DSTDOUT_DEBUG_MSG
