# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += tls_demo
tls_demo-objs-y   := src/main.c src/reset_prov_helper.c
tls_demo-objs-y   += src/tls-demo.c src/tls-httpc.c src/ntpc.c
tls_demo-cflags-y := -I$(d)/src -D APPCONFIG_DEBUG_ENABLE=1

tls_demo-objs-$(TLS_DEMO_MDNS_ENABLE)   += src/mdns_helper.c
tls_demo-cflags-$(TLS_DEMO_MDNS_ENABLE) += -DAPPCONFIG_MDNS_ENABLE

tls_demo-objs-$(TLS_DEMO_WPS_ENABLE)    += src/wps_helper.c
tls_demo-cflags-$(TLS_DEMO_WPS_ENABLE)  += -DAPPCONFIG_WPS_ENABLE

tls_demo-ftfs-y := tls_demo.ftfs
tls_demo-ftfs-dir-y := $(d)/www
tls_demo-ftfs-api-y := 100

# Applications could also define custom board files if required using following:
#tls_demo-board-y := /path/to/boardfile
#tls_demo tls_demo-objs-y += src/tls-demo.c src/tls-httpc.c src/ntpc.c tls_demo-objs-$(TLS_DEMO_MDNS_ENABLE) += src/mdns_helper.c tls_demo-cflags-$(TLS_DEMO_MDNS_ENABLE) += -DAPPCONFIG_MDNS_ENABLE tls_demo-objs-$(TLS_DEMO_WPS_ENABLE) += src/wps_helper.c tls_demo-cflags-$(TLS_DEMO_WPS_ENABLE) += -DAPPCONFIG_WPS_ENABLE-linkerscrpt-y := /path/to/linkerscript
