# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += server_demo
server_demo-objs-y := src/main.c src/server.c

# Applications could also define custom board files if required using following:
#server_demo-board-y := /path/to/boardfile
#server_demo-linkerscrpt-y := /path/to/linkerscript
