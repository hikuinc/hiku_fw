# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += deepsleep_pm4
deepsleep_pm4-objs-y   := src/main.c
# Applications could also define custom board files if required using following:
#deepsleep_pm4-board-y := /path/to/boardfile
#deepsleep_pm4-linkerscrpt-y := /path/to/linkerscript
