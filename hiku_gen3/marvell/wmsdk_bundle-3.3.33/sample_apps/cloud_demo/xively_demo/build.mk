# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += xively_demo
xively_demo-objs-y := src/main.c
# Applications could also define custom board files if required using following:
#xively_demo-board-y := /path/to/boardfile
#xively_demo-linkerscrpt-y := /path/to/linkerscript
