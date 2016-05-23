# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += usb_host_cdc
usb_host_cdc-objs-y := src/main.c
usb_host_cdc-cflags-y := -I$(d)/src/

usb_host_cdc-linkerscript-$(CONFIG_CPU_MC200) := $(d)/../mc200_usb.ld
# Applications could also define custom board files if required using following:
#usb_host_cdc-board-y := /path/to/boardfile
