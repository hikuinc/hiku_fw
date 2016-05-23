# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += qa2
qa2-objs-y :=  src/main.c src/aes_test_main.c

# Applications could also define custom board files if required using following:
#qa2-board-y := /path/to/boardfile
#qa2-linkerscrpt-y := /path/to/linkerscript
