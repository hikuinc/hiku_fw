# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += qa1
qa1-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#qa1-board-y := /path/to/boardfile

subdir-y += test_subdir
#qa1 subdir-y += test_subdir-linkerscrpt-y := /path/to/linkerscript
