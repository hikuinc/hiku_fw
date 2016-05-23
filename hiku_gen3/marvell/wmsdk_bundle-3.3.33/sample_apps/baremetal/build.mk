# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += baremetal
baremetal-objs-y :=  src/main.c
# Applications could also define custom board files if required using following:
#baremetal-board-y := /path/to/boardfile
#baremetal-linkerscrpt-y := /path/to/linkerscript
