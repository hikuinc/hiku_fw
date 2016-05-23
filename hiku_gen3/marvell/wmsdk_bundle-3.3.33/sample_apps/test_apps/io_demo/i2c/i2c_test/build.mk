# Copyright (C) 2008-2016, Marvell International Ltd.
# All Rights Reserved.

exec-y += i2c_test
i2c_test-objs-y :=  src/main.c src/i2c.c
# Applications could also define custom board files if required using following:
#i2c_test-board-y := /path/to/boardfile
#i2c_test-linkerscrpt-y := /path/to/linkerscript
