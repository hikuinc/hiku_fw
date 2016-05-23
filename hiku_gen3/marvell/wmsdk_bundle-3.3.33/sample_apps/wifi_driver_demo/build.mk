# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += wifi_driver_demo
wifi_driver_demo-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#wifi_driver_demo-board-y := /path/to/boardfile
#wifi_driver_demo-linkerscrpt-y := /path/to/linkerscript
