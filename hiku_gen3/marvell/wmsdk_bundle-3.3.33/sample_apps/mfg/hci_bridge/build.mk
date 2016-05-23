# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y := hci_bridge
hci_bridge-objs-y := src/main.c
hci_bridge-cflags-y := -I./src -D APPCONFIG_DEBUG_ENABLE=1
# Applications could also define custom board files if required using following:
#hci_bridge-board-y := /path/to/boardfile
