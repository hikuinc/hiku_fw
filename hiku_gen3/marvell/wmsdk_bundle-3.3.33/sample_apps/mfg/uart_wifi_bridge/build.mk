# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += uart_wifi_bridge
uart_wifi_bridge-objs-y := src/main.c
uart_wifi_bridge-cflags-y := -I$(d)/src -D APPCONFIG_DEBUG_ENABLE=1
uart_wifi_bridge-cflags-$(CONFIG_BT_SUPPORT) += -DAPPCONFIG_BT_SUPPORT


# Applications could also define custom board files if required using following:
#uart_wifi_bridge-board-y := /path/to/boardfile
#uart_wifi_bridge uart_wifi_bridge-cflags-$(CONFIG_BT_SUPPORT) += -DAPPCONFIG_BT_SUPPORT-linkerscrpt-y := /path/to/linkerscript
