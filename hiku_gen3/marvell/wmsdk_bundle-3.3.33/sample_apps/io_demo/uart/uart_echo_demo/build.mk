# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += uart_echo_demo
uart_echo_demo-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#uart_echo_demo-board-y := /path/to/boardfile
#uart_echo_demo-linkerscrpt-y := /path/to/linkerscript
