# Copyright (C) 2008-2015 Marvell International Ltd.
# All Rights Reserved.

exec-y += test_subdir
test_subdir-objs-y := src/main.c
# Applications could also define custom linker files if required using following:
#test_subdir-linkerscript-y := /path/to/linkerscript
# Applications could also define custom board files if required using following:
#test_subdir-board-y := /path/to/boardfile
