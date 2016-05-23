# Copyright (C) 2008-2016, Marvell International Ltd.
# All Rights Reserved.

global-cflags-y +=-Isdk/src/incl/platform/net/lwip       \
                  -I$(d)/src/include/        \
                  -I$(d)/src/include/ipv4    \
                  -I$(d)/src/include/ipv6    \
                  -I$(d)/contrib/port/FreeRTOS/wmsdk/

-include $(d)/build.lwip.mk

