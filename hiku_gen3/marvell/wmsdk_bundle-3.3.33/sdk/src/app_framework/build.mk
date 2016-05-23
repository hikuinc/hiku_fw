# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

libs-y += libapp_framework

libapp_framework-objs-y := app_main.c app_network_config.c app_fs.c app_tls.c
libapp_framework-objs-y += app_provisioning.c app_network_mgr.c app_ctrl.c
libapp_framework-objs-y += app_sys_http_handlers.c app_psm.c app_reboot.c app_cli.c
libapp_framework-objs-y += app_mdns_services.c app_httpd.c app_ezconnect_provisioning.c

libapp_framework-objs-$(CONFIG_AUTO_TEST_BUILD) +=  app_test_http_handlers.c
libapp_framework-cflags-$(CONFIG_AUTO_TEST_BUILD) +=  -DCONFIG_AUTO_TEST_BUILD

libapp_framework-objs-$(CONFIG_P2P) += app_p2p.c
