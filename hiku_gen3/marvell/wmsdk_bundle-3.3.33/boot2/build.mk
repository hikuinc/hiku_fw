# Copyright (C) 2016 Marvell International Ltd.
# All Rights Reserved.

ifneq ($(arch_name-y),)
subdir-y += src/$(arch_name-y)
endif

subdir-y += bh_generator.mk
