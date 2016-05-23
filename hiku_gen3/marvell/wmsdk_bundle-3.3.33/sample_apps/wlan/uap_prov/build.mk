# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

UAP_PROV_CONFIG_MICRO_AP_SECURITY_ENABLE=y
UAP_PROV_CONFIG_APP_LEVEL_SECURITY_ENABLE=n
UAP_PROV_CONFIG_HTTPS_ENABLE=y

exec-y += uap_prov
uap_prov-objs-y   := src/main.c src/reset_prov_helper.c src/wps_helper.c
uap_prov-cflags-y := -I$(d)/src

UAP_PROV_MDNS_ENABLE=y
uap_prov-objs-$(UAP_PROV_MDNS_ENABLE)   += src/mdns_helper.c
uap_prov-cflags-$(UAP_PROV_MDNS_ENABLE) += -DAPPCONFIG_MDNS_ENABLE

uap_prov-cflags-$(UAP_PROV_CONFIG_MICRO_AP_SECURITY_ENABLE) \
	+= -DUAP_PROV_CONFIG_MICRO_AP_SECURITY_ENABLE
uap_prov-cflags-$(UAP_PROV_CONFIG_APP_LEVEL_SECURITY_ENABLE) \
	+= -DUAP_PROV_CONFIG_APP_LEVEL_SECURITY_ENABLE
ifeq ($(CONFIG_ENABLE_HTTPS_SERVER),y)
uap_prov-cflags-$(UAP_PROV_CONFIG_HTTPS_ENABLE) \
	+= -DUAP_PROV_CONFIG_HTTPS_ENABLE
endif

# Enable for debuggin
#uap_prov-cflags-y += -D APPCONFIG_DEBUG_ENABLE

uap_prov-ftfs-y := uap_prov.ftfs
uap_prov-ftfs-dir-y                          := $(d)/www
uap_prov-ftfs-api-y                      := 100

ifeq ($(UAP_PROV_CONFIG_APP_LEVEL_SECURITY_ENABLE),y)
	uap_prov-ftfs-y     := uap_prov.ftfs
	uap_prov-ftfs-dir-y := $(d)/www-secure
	uap_prov-ftfs-api-y := 100
else
	uap_prov-ftfs-y := uap_prov.ftfs
	uap_prov-ftfs-dir-y := $(d)/www
	uap_prov-ftfs-api-y := 100
endif

# Applications could also define custom board files if required using following:
#uap_prov-board-y := /path/to/boardfile
#uap_prov uap_prov-objs-$(UAP_PROV_MDNS_ENABLE) += src/mdns_helper.c uap_prov-cflags-$(UAP_PROV_MDNS_ENABLE) += -DAPPCONFIG_MDNS_ENABLE += -DUAP_PROV_CONFIG_MICRO_AP_SECURITY_ENABLE += -DUAP_PROV_CONFIG_APP_LEVEL_SECURITY_ENABLE += -DUAP_PROV_CONFIG_HTTPS_ENABLE #uap_prov-cflags-y += -D APPCONFIG_DEBUG_ENABLE-linkerscrpt-y := /path/to/linkerscript
