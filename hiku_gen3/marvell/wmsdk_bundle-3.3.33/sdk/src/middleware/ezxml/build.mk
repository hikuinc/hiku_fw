# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

libs-y += libezxml
libezxml-objs-y                     := ezxml.c
libezxml-objs-$(CONFIG_EZXML_TESTS) += test.c
