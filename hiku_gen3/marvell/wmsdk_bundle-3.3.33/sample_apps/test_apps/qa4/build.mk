# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += qa4
qa4-objs-y := src/main.c src/reset_prov_helper.c src/wps_helper.c
qa4-objs-y += src/mdns_helper.c
qa4-cflags-y := -DAPPCONFIG_MDNS_ENABLE -I$(d)/src/
# Enable for debugging
#qa4-cflags-y += -D APPCONFIG_DEBUG_ENABLE

qa4-ftfs-y := qa4.ftfs
qa4-ftfs-dir-y     := $(d)/www
qa4-ftfs-api-y := 100
# Applications could also define custom board files if required using following:
#qa4-board-y := /path/to/boardfile
#qa4 qa4-objs-y += src/mdns_helper.c #qa4-cflags-y += -D APPCONFIG_DEBUG_ENABLE-linkerscrpt-y := /path/to/linkerscript
