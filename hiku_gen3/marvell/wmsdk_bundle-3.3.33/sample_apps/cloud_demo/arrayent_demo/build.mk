# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += arrayent_demo
arrayent_demo-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#arrayent_demo-board-y := /path/to/boardfile
#arrayent_demo-linkerscrpt-y := /path/to/linkerscript
