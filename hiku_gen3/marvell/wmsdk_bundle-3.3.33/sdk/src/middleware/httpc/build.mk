# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

libs-y += libhttpclient
libhttpclient-objs-y                      := httpc.c
libhttpclient-objs-$(CONFIG_ENABLE_TESTS) += httpc_tests.c
