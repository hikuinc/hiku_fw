# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += adc_demo
adc_demo-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#adc_demo-board-y := /path/to/boardfile
#adc_demo-linkerscrpt-y := /path/to/linkerscript
