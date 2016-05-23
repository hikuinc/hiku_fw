# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += assembly_demo
assembly_demo-objs-y := src/main.c src/add_them.s
# Applications could also define custom board files if required using following:
#assembly_demo-board-y := /path/to/boardfile
#assembly_demo-linkerscrpt-y := /path/to/linkerscript
