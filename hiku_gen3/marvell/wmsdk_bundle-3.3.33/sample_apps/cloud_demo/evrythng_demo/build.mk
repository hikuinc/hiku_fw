# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += evrythng_demo
evrythng_demo-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#evrythng_demo-board-y := /path/to/boardfile
#evrythng_demo-linkerscrpt-y := /path/to/linkerscript
