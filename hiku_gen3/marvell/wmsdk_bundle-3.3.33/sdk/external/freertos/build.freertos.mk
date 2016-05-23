# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

libs-y += libfreertos
disable-lto-for += libfreertos
libfreertos-objs-y := Source/list.c Source/queue.c Source/tasks.c Source/event_groups.c
libfreertos-objs-y += Source/croutine.c Source/timers.c
libfreertos-objs-y += Source/portable/MemMang/heap_4.c Source/FreeRTOS-openocd.c

libfreertos-objs-$(tc-cortex-m4-y) += Source/portable/$(tc-env)/ARM_CM4F/port.c
libfreertos-objs-$(tc-cortex-m3-y) += Source/portable/$(tc-env)/ARM_CM3/port.c

libfreertos-cflags-$(DEBUG_HEAP) += -DDEBUG_HEAP
libfreertos-cflags-$(CONFIG_ENABLE_STACK_OVERFLOW_CHECK) += -DCONFIG_ENABLE_STACK_OVERFLOW_CHECK
libfreertos-cflags-$(CONFIG_ENABLE_ASSERTS) += -DCONFIG_ENABLE_ASSERT
