# Copyright 2008-2015, Marvell International Ltd.

libs-y += libbn

libbn-objs-y :=  bn_add.c bn_asm.c bn_ctx.c
libbn-objs-y +=  bn_div.c bn_exp.c bn_lib.c
libbn-objs-y +=  bn_mul.c bn_shift.c bn_sqr.c
