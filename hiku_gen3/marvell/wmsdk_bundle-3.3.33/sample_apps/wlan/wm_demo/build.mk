# Copyright (C) 2008-2015 Marvell International Ltd.
# All Rights Reserved.
#

WM_DEMO_CONFIG_MDNS_ENABLE=y

exec-y += wm_demo
wm_demo-objs-y := src/main.c src/reset_prov_helper.c src/led_indications.c src/utility.c
wm_demo-cflags-y := -I$(d)/src -DAPPCONFIG_DEBUG_ENABLE=1

wm_demo-objs-$(WM_DEMO_CONFIG_MDNS_ENABLE) += src/mdns_helper.c

wm_demo-cflags-$(WM_DEMO_CONFIG_MDNS_ENABLE) += -DAPPCONFIG_MDNS_ENABLE


wm_demo-ftfs-y := wm_demo.ftfs
wm_demo-ftfs-dir-y     := $(d)/www
wm_demo-ftfs-api-y := 100

# Applications could also define custom board files if required using following:
#wm_demo-board-y := /path/to/boardfile
#wm_demo wm_demo-objs-$(WM_DEMO_CONFIG_MDNS_ENABLE) += src/mdns_helper.c wm_demo-cflags-$(WM_DEMO_CONFIG_MDNS_ENABLE) += -DAPPCONFIG_MDNS_ENABLE-linkerscrpt-y := /path/to/linkerscript
