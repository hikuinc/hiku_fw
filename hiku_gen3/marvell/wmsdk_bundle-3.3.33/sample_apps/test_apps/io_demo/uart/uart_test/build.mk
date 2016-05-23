# Copyright (C) 2008-2016, Marvell International Ltd.
# All Rights Reserved.

exec-y += uart_test
uart_test-objs-y :=  src/main.c src/uart.c
# Applications could also define custom board files if required using following:
#uart_test-board-y := /path/to/boardfile
#uart_test-linkerscrpt-y := /path/to/linkerscript
