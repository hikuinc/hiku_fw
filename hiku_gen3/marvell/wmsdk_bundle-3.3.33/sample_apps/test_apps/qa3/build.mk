# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += qa3

qa3-objs-y   := src/main.c src/reset_prov_helper.c src/wps_helper.c
qa3-cflags-y := -I$(d)/src

QA3_MDNS_ENABLE=y
qa3-objs-$(QA3_MDNS_ENABLE)   += src/mdns_helper.c
qa3-cflags-$(QA3_MDNS_ENABLE) += -DAPPCONFIG_MDNS_ENABLE

# Enable for debugging
#qa3-cflags-y += -D APPCONFIG_DEBUG_ENABLE

qa3-ftfs-y := qa3.ftfs
qa3-ftfs-dir-y     := $(d)/www
qa3-ftfs-api-y := 100

# Applications could also define custom board files if required using following:
#qa3-board-y := /path/to/boardfile
#qa3 qa3-objs-$(QA3_MDNS_ENABLE) += src/mdns_helper.c qa3-cflags-$(QA3_MDNS_ENABLE) += -DAPPCONFIG_MDNS_ENABLE #qa3-cflags-y += -D APPCONFIG_DEBUG_ENABLE-linkerscrpt-y := /path/to/linkerscript
