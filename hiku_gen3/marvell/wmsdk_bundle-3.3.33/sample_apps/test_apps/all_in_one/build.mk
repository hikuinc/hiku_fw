# Copyright (C) 2008-2015 Marvell International Ltd.
# All Rights Reserved.
#

include $(d)/config.mk

exec-y += all_in_one
all_in_one-objs-y := src/main.c src/reset_prov_helper.c src/wmcloud.c src/wmcloud_helper.c src/aio_wps_cli.c
all_in_one-cflags-y := -I$(d)/src -DAPPCONFIG_DEBUG_ENABLE=1 -DAPPCONFIG_DEMO_CLOUD=1

all_in_one-objs-$(ALL_IN_ONE_LONG_POLL_CLOUD)      += src/wmcloud_lp.c src/aio_cloud.c
all_in_one-objs-$(ALL_IN_ONE_WEBSOCKET_CLOUD)      += src/wmcloud_ws.c src/aio_cloud.c
all_in_one-objs-$(ALL_IN_ONE_XIVELY_CLOUD)         += src/wmcloud_xively.c src/aio_xively_cloud.c
all_in_one-objs-$(ALL_IN_ONE_ARRAYENT_CLOUD)       += src/wmcloud_arrayent.c src/aio_arrayent_cloud.c

all_in_one-objs-$(ALL_IN_ONE_CONFIG_MDNS_ENABLE)   += src/mdns_helper.c
all_in_one-cflags-$(ALL_IN_ONE_CONFIG_MDNS_ENABLE) += -DAPPCONFIG_MDNS_ENABLE

all_in_one-objs-$(ALL_IN_ONE_CONFIG_WPS_ENABLE)    += src/wps_helper.c
all_in_one-cflags-$(ALL_IN_ONE_CONFIG_WPS_ENABLE)  += -DAPPCONFIG_WPS_ENABLE

all_in_one-cflags-$(ALL_IN_ONE_CONFIG_HTTPS_CLOUD) += -DAPPCONFIG_HTTPS_CLOUD

all_in_one-objs-$(ALL_IN_ONE_CONFIG_PM_ENABLE)     += src/power_mgr_helper.c
all_in_one-cflags-$(ALL_IN_ONE_CONFIG_PM_ENABLE)   += -DAPPCONFIG_PM_ENABLE


all_in_one-cflags-$(ALL_IN_ONE_CONFIG_HTTPS_SERVER)+= -DAPPCONFIG_HTTPS_SERVER

all_in_one-linkerscript-$(ALL_IN_ONE_CONFIG_OVERLAY_ENABLE)  := $(d)/src/aio_overlay.ld
all_in_one-objs-$(ALL_IN_ONE_CONFIG_OVERLAY_ENABLE)+= src/overlays.c src/aio_overlays.c
all_in_one-cflags-$(ALL_IN_ONE_CONFIG_OVERLAY_ENABLE)+= -DAPPCONFIG_OVERLAY_ENABLE

all_in_one-cflags-$(ALL_IN_ONE_CONFIG_PROV_EZCONNECT) += -DAPPCONFIG_PROV_EZCONNECT


all_in_one-ftfs-y := all_in_one.ftfs
all_in_one-ftfs-dir-y     := $(d)/www
all_in_one-ftfs-api-y := 100

# Applications could also define custom board files if required using following:
#all_in_one-board-y := /path/to/boardfile
