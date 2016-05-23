# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += os_demo
os_demo-objs-y :=  src/main.c src/mutex_demo.c
# Applications could also define custom board files if required using following:
#os_demo-board-y := /path/to/boardfile
#os_demo-linkerscrpt-y := /path/to/linkerscript
