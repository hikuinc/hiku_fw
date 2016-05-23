# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += pm_mcu_wifi_demo
pm_mcu_wifi_demo-objs-y   := src/main.c
pm_mcu_wifi_demo-cflags-y := -I$(d)/src
# Applications could also define custom board files if required using following:
#pm_mcu_wifi_demo-board-y := /path/to/boardfile
#pm_mcu_wifi_demo-linkerscrpt-y := /path/to/linkerscript
