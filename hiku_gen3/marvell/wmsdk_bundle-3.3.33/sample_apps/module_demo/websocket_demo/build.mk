# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += websocket_demo
websocket_demo-objs-y :=  src/main.c src/websocket.c

# Applications could also define custom board files if required using following:
#websocket_demo-board-y := /path/to/boardfile
#websocket_demo-linkerscrpt-y := /path/to/linkerscript
