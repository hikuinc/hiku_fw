# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += usb_host_uac
usb_host_uac-objs-y := src/main.c
usb_host_uac-linkerscript-$(CONFIG_CPU_MC200) := $(d)/../mc200_usb.ld
usb_host_uac-cflags-y := -I$(d)/src/
# Applications could also define custom board files if required using following:
#usb_host_uac-board-y := /path/to/boardfile
