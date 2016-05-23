# Copyright (C) 2008-2016, Marvell International Ltd.
# All Rights Reserved.

prj_name := uart_dma_rx_demo

exec-y += $(prj_name)
$(prj_name)-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#$(prj_name)-board-y := /path/to/boardfile
#$(prj_name)-linkerscrpt-y := /path/to/linkerscript
