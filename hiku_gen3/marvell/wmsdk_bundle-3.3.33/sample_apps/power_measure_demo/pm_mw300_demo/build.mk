# Copyright (C) 2008-2016, Marvell International Ltd.
# All Rights Reserved.

exec-y += pm_mw300_demo
pm_mw300_demo-objs-y :=   src/pm_mw300_demo.c
# Applications could also define custom board files if required using following:
#pm_mw300_demo-board-y := /path/to/boardfile

pm_mw300_demo-linkerscript-y := $(d)/src/pm_mw300_demo.ld


