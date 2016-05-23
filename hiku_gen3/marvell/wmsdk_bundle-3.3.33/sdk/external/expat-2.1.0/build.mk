# Copyright (C) 2008-2016, Marvell International Ltd.
# All Rights Reserved.

expat_version := 2.1.0

libs-y += libexpat-$(expat_version)

libexpat-$(expat_version)-objs-y := lib/xmlparse.c  lib/xmlrole.c  lib/xmltok.c
libexpat-$(expat_version)-objs-y += lib/xmltok_impl.c  lib/xmltok_ns.c

global-cflags-y += -DHAVE_EXPAT_CONFIG_H -I$(d)/lib -I$(d)

# libapp_framework-cflags-y :=
