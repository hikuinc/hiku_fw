# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += i2c_simple_master_demo
i2c_simple_master_demo-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#i2c_simple_master_demo-board-y := /path/to/boardfile
#i2c_simple_master_demo-linkerscrpt-y := /path/to/linkerscript
