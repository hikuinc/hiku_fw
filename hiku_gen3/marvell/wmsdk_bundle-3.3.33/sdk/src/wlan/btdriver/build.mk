# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

libs-y += libbtdriver
libbtdriver-objs-y := bt.c
libbtdriver-cflags-y := -I$(d)
