# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += rw_lock_demo
rw_lock_demo-objs-y := src/main.c src/rp_rw_demo.c src/rw_lock_demo.c
# Applications could also define custom board files if required using following:
#rw_lock_demo-board-y := /path/to/boardfile
#rw_lock_demo-linkerscrpt-y := /path/to/linkerscript
