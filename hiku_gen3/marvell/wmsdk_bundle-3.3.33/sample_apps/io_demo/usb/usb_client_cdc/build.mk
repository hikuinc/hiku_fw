# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += usb_client_cdc
usb_client_cdc-objs-y += src/main.c
usb_client_cdc-cflags-y := -I$(d)/src
usb_client_cdc-linkerscript-$(CONFIG_CPU_MC200) := $(d)/../mc200_usb.ld
# Applications could also define custom board files if required using following:
#usb_client_cdc-board-y := /path/to/boardfile
