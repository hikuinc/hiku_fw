# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

libs-y += libpoly1305
libpoly1305-objs-y   := poly1305-donna.c
libpoly1305-cflags-y := -DPOLY1305_32BITS
