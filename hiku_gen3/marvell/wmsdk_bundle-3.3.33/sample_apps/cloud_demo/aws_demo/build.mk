# Copyright (C) 2008-2015 Marvell International Ltd.
# All Rights Reserved.
#

exec-y += aws_demo
aws_demo-objs-y := src/main.c
aws_demo-cflags-y := -I$(d)/src -DAPPCONFIG_DEBUG_ENABLE=1

ifneq ($(wildcard $(d)/www),)
aws_demo-ftfs-y :=  aws_demo.ftfs
aws_demo-ftfs-dir-y     := $(d)/www
aws_demo-ftfs-api-y := 100
endif
#aws_demo-linkerscrpt-y := /path/to/linkerscript
