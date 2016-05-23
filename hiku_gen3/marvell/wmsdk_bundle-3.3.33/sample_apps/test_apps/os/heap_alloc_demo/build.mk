# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += heap_alloc_demo
heap_alloc_demo-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#heap_alloc_demo-board-y := /path/to/boardfile
#heap_alloc_demo-linkerscrpt-y := /path/to/linkerscript
