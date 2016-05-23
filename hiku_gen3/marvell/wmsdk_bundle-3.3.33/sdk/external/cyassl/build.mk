# Copyright (C) 2008-2016, Marvell International Ltd.
# All Rights Reserved.

# Default CyaSSL Feature Pack is FP0
CYASSL_FEATURE_PACK ?= fp0

# Add our CFLAGS, too many directories for me to be comfortable with
global-cflags-y += \
		-I$(d)/cyassl/ctaocrypt                            \
		-I$(d)/                                            \
		-I$(d)/cyassl                                      \
		-I$(d)/cyassl/cyassl 				   \
		-I$(d)/wolfssl 					   \
		-I$(d)/wolfssl/wolfcrypt 			   \


cyassl_dir_lto-y := non-lto
cyassl_dir_lto-$(CONFIG_ENABLE_LTO) := lto

# set the  cyassl feature pack value
# passed by user by default
#
cyassl_feature_pack-y := $(CYASSL_FEATURE_PACK)

# if "CONFIG_ENABLE_CYASSL_DEBUG" is set to y select debug feature pack
# for corressponding value send by user or default fo0 debug

cyassl_feature_pack-$(CONFIG_ENABLE_CYASSL_DEBUG) := ${cyassl_feature_pack-$(CONFIG_ENABLE_CYASSL_DEBUG):%_debug=%}

# even if user sets "CONFIG_ENABLE_CYASSL_DEBUG" to y and also set
# CYASSL_FEATURE_PACK=fpx_debug it is handled in above line

cyassl_feature_pack-$(CONFIG_ENABLE_CYASSL_DEBUG) := $(cyassl_feature_pack-$(CONFIG_ENABLE_CYASSL_DEBUG))_debug

ifneq ($(MRVL_INTERNAL_BUILD),y)
 global-cflags-y +=-I$(d)/cyassl/wmsdk-configs/$(cyassl_feature_pack-y)
endif
global-prebuilt-libs-y += $(d)/libs/$(arch_name-y)/$(cyassl_dir_lto-y)/libcyassl_$(cyassl_feature_pack-y).a $(d)/libs/$(arch_name-y)/$(cyassl_dir_lto-y)/libctaocrypt_$(cyassl_feature_pack-y).a

