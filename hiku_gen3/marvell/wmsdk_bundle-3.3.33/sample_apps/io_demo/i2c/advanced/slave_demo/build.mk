# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += i2c_adv_slave_demo
i2c_adv_slave_demo-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#i2c_adv_slave_demo-board-y := /path/to/boardfile
#i2c_adv_slave_demo-linkerscrpt-y := /path/to/linkerscript
