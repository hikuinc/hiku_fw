# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += webhandler_demo
webhandler_demo-objs-y   := src/main.c src/json_webhandler.c src/text_webhandler.c
webhandler_demo-cflags-y := -I$(d)/src

# Applications could also define custom board files if required using following:
#webhandler_demo-board-y := /path/to/boardfile
#webhandler_demo-linkerscrpt-y := /path/to/linkerscript
