# Copyright (C) 2015 Marvell International Ltd.

libs-y += libusb
global-cflags-y += -I$(d)/
libusb-objs-y := src/core/asic_usb.c src/core/vusbhs_dev_cncl.c src/core/vusbhs_dev_deinit.c
libusb-objs-y += src/core/vusbhs_dev_main.c src/core/vusbhs_dev_shut.c src/core/vusbhs_dev_utl.c
libusb-objs-y += src/core/dev_cncl.c src/core/dev_ep_deinit.c src/core/dev_main.c
libusb-objs-y += src/core/dev_recv.c src/core/dev_send.c src/core/dev_shut.c
libusb-objs-y += src/core/dev_utl.c src/common/agLinkedList.c src/common/usb_list.c
libusb-objs-y += src/device_api/usbdevice.c src/device_class/usbcdcdevice.c src/device_class/usbmscdevice.c
libusb-objs-y += src/device_class/usbhiddevice.c src/device_api/usbsysinit.c src/device_class/handle_MS_BOT_protocol.c
libusb-objs-y += src/device_class/MS_SCSI_read_cmds.c src/device_class/midia_sd_.c src/device_class/MS_SCSI_other_cmds.c
libusb-objs-y += src/device_class/MS_SCSI_top.c src/device_class/MS_SCSI_write_cmds.c


