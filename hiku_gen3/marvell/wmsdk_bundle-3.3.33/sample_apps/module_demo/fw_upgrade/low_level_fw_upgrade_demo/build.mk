# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += low_level_fw_upgrade_demo
low_level_fw_upgrade_demo-objs-y := src/main.c
low_level_fw_upgrade_demo-cflags-y                       := -I$(d)/src -D APPCONFIG_DEBUG_ENABLE=1

# Applications could also define custom board files if required using following:
#low_level_fw_upgrade_demo-board-y := /path/to/boardfile
#low_level_fw_upgrade_demo-linkerscrpt-y := /path/to/linkerscript
