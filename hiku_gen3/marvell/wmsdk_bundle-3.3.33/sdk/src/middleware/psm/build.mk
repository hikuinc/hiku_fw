# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

libs-y += libpsm
libpsm-objs-y := psm-v2.c psm-wrapper.c psm-legacy.c
libpsm-objs-y += psm-v2-secure.c tests/psm-test-main.c
libpsm-objs-y += psm-utils.c

