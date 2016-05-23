# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += httpd_secure_demo
httpd_secure_demo-objs-y   := src/main.c src/text_webhandler.c
httpd_secure_demo-cflags-y := -I$(d)/src



# Applications could also define custom board files if required using following:
#httpd_secure_demo-board-y := /path/to/boardfile
#httpd_secure_demo-linkerscrpt-y := /path/to/linkerscript
