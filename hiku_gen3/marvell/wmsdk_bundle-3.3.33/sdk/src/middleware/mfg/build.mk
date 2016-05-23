# Copyright 2008-2015, Marvell International Ltd.

libs-y += libmfg

libmfg-objs-y :=  mfg_psm.c

libmfg-cflags-y := -I$(d)
