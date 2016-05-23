
# Copyright (C) 2008-2016 Marvell International Ltd.
# All Rights Reserved.

exec-y += deepsleep_pm3
deepsleep_pm3-objs-y := src/main.c
# Applications could also define custom linker files if required using
following:
# Applications could also define custom board files if required using following
#deepsleep_pm3-board-y := /path/to/boardfile
