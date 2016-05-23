# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += gpio_demo
gpio_demo-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#gpio_demo-board-y := /path/to/boardfile
#gpio_demo-linkerscrpt-y := /path/to/linkerscript
