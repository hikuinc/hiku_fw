# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += psm_demo
psm_demo-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#psm_demo-board-y := /path/to/boardfile
#psm_demo-linkerscrpt-y := /path/to/linkerscript
