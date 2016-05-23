# Copyright (C) 2008-2016 Marvell International Ltd.
# All Rights Reserved.

exec-y += ssp_test
ssp_test-objs-y := src/main.c src/ssp.c
#ssp_test-linkerscrpt-y := /path/to/linkerscript
