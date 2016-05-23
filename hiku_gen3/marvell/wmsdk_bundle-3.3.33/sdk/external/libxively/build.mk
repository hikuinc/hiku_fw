# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

XI_USER_AGENT ?= "libxively-lwip-experimental/0.1.x-detached"
XI_WARN_CFLAGS := -Wall -Wextra -Wno-unused-parameter -Wno-unknown-pragmas

libs-y += libxively
global-cflags-y += -I$(d)/src/
libxively-objs-y := src/libxively/xi_time.o src/libxively/xi_helpers.o src/libxively/xively.o \
                    src/libxively/xi_http_layer_constants.o src/libxively/xi_generator.o      \
                    src/libxively/xi_globals.o src/libxively/xi_csv_layer.o                   \
                    src/libxively/xi_http_layer.o src/libxively/xi_stated_sscanf.o            \
                    src/libxively/xi_err.o src/libxively/io/posix/posix_io_layer.o
libxively-cflags-y := -DXI_IO_LAYER_POSIX_COMPAT=1 -DXI_IO_LAYER_POSIX_DISABLE_TIMEOUT -std=gnu99 -fgnu89-inline
libxively-cflags-y += -DXI_USER_AGENT='$(XI_USER_AGENT)' $(XI_WARN_CFLAGS) -I$(d)/src/libxively/ -I$(d)/src/libxively/io/posix/
