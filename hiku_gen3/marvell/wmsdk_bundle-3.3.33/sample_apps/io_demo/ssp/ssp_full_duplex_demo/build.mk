# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += ssp_full_duplex_demo
ssp_full_duplex_demo-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#ssp_full_duplex_demo-board-y := /path/to/boardfile
#ssp_full_duplex_demo-linkerscrpt-y := /path/to/linkerscript
