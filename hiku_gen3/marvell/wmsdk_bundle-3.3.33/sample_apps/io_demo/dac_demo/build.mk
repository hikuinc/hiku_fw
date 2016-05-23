# Copyright (C) 2008-2016, Marvell International Ltd.
# All Rights Reserved.

exec-y += dac_demo
dac_demo-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#dac_demo-board-y := /path/to/boardfile
#dac_demo-linkerscrpt-y := /path/to/linkerscript
