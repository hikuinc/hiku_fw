# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += pm_i2c_slave_demo
pm_i2c_slave_demo-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#pm_i2c_slave_demo-board-y := /path/to/boardfile
#pm_i2c_slave_demo-linkerscrpt-y := /path/to/linkerscript
