# Copyright (C) 2008-2015 Marvell International Ltd.
# All Rights Reserved.

exec-y += hello_world
hello_world-objs-y := src/main.c
# Applications could also define custom linker files if required using following:
#hello_world-linkerscript-y := /path/to/linkerscript
# Applications could also define custom board files if required using following:
#hello_world-board-y := /path/to/boardfile
