# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += cert_demo
cert_demo-objs-y = src/http_handlers.c src/main.c src/event_handler.c
cert_demo-cflags-y := -I $(d)/src
cert_demo-cflags-$(APP_DEBUG_ENABLE) += -DAPPCONFIG_DEBUG_ENABLE



# Applications could also define custom board files if required using following:
#cert_demo-board-y := /path/to/boardfile
#cert_demo cert_demo-cflags-$(APP_DEBUG_ENABLE) += -DAPPCONFIG_DEBUG_ENABLE-linkerscrpt-y := /path/to/linkerscript
