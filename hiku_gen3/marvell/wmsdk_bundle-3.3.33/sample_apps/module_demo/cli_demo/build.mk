# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += cli_demo
cli_demo-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#cli_demo-board-y := /path/to/boardfile
#cli_demo-linkerscrpt-y := /path/to/linkerscript
