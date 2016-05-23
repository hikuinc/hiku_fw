# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

libs-y += libnet
libnet-objs-y := net.c netif.c bcast_filter.c
# This internal knowledge of the location of wifi-driver should be removed
libnet-cflags-y := -Isdk/src/wlan/wifidriver -Isdk/src/wlan/wifidriver/incl
