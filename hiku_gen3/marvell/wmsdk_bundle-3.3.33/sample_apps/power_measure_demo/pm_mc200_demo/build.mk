# Copyright (C) 2008-2016, Marvell International Ltd.
# All Rights Reserved.

exec-y += pm_mc200_demo
pm_mc200_demo-objs-y :=  src/main.c src/peripheralsOn.c src/peripheralsOff.c
# Applications could also define custom board files if required using following:
#pm_mc200_demo-board-y := /path/to/boardfile

pm_mc200_demo-linkerscript-y := $(d)/src/pm_mc200_demo.ld

