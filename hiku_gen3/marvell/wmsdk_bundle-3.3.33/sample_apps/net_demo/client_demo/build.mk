# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += client_demo
client_demo-objs-y := src/main.c src/client.c

# Applications could also define custom board files if required using following:
#client_demo-board-y := /path/to/boardfile
#client_demo-linkerscrpt-y := /path/to/linkerscript
