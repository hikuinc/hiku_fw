# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += ssp_slave_demo
ssp_slave_demo-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#ssp_slave_demo-board-y := /path/to/boardfile
#ssp_slave_demo-linkerscrpt-y := /path/to/linkerscript
