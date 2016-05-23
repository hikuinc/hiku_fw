# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

TOOLCHAIN ?= GCC

ifeq ($(TOOLCHAIN), GCC)
  include build/toolchains/toolchain_gcc.mk
endif
