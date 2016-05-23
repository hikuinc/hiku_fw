# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += perf_demo
perf_demo-objs-y   := src/main.c src/event_handler.c src/perf_demo_drv.c
perf_demo-cflags-y := -I$(d)/src -DFW_VERSION=\"$(FW_VERSION)\" -DAPP_DEBUG_PRINT -DPERF_DEMO_APP_DEBUG_PRINT -DFTFS_API_VERSION=114

perf_demo-ftfs-y := perf_demo.ftfs
perf_demo-ftfs-dir-y     := $(d)/www
perf_demo-ftfs-api-y := 114


# Applications could also define custom board files if required using following:
#perf_demo-board-y := /path/to/boardfile
#perf_demo-linkerscrpt-y := /path/to/linkerscript
