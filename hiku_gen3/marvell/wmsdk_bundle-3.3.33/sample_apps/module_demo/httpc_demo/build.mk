# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += httpc_demo
httpc_demo-objs-y := \
		src/main.c \
		src/httpc_post.c \
		src/httpc_post_callback.c  \
		src/httpc_secure_post.c \
		src/wikipedia-server.cert.pem.c \
		src/httpc_get.c \
		src/set_device_time.c \
		src/httpc_secure_get.c

httpc_demo-cflags-y := -I$(d)/src



# Applications could also define custom board files if required using following:
#httpc_demo-board-y := /path/to/boardfile
#httpc_demo-linkerscrpt-y := /path/to/linkerscript
