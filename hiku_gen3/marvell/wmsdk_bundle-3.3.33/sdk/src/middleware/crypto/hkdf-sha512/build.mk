# Copyright 2008-2015, Marvell International Ltd.

libs-y += libhkdf-sha512

libhkdf-sha512-objs-y :=  hkdf.c hkdf-hmac.c sha384-512.c
libhkdf-sha512-objs-y +=  usha.c
