# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += ssp_master_demo
ssp_master_demo-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#ssp_master_demo-board-y := /path/to/boardfile
#ssp_master_demo-linkerscrpt-y := /path/to/linkerscript
