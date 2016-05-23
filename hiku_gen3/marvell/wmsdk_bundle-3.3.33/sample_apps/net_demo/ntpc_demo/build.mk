# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += ntpc_demo
ntpc_demo-objs-y := src/main.c src/ntpc.c

# Applications could also define custom board files if required using following:
#ntpc_demo-board-y := /path/to/boardfile
#ntpc_demo-linkerscrpt-y := /path/to/linkerscript
