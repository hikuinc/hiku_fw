# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

libs-y += libed25519
libed25519-objs-y   := ed25519.c
libed25519-cflags-y := -DED25519_CUSTOMRNG -DED25519_CUSTOMHASH -DED25519_FLASH_DATA
