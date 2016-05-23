# Copyright (C) 2008-2015 Marvell International Ltd.
# All Rights Reserved.

exec-y += mutex_demo
mutex_demo-objs-y := src/main.c src/mutex_demo.c
# Applications could also define custom board files if required using following:
#mutex_demo-board-y := /path/to/boardfile
#mutex_demo-linkerscrpt-y := /path/to/linkerscript
