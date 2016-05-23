# Copyright (C) 2008-2016, Marvell International Ltd.
# All Rights Reserved.
#
# Description:
# ------------
# This file, wmsdk_rules.mk contains call to following rules/functions:
#
# 	mfg binary creation		(create_mfg_bin)
# 	upg binary preprocessing	(check_creation_upg_bin)
# 	upg binary creation		(create_upg_bin)

# Pull in the standard set of rules
include build/rules.mk
include build/extended_rules.mk
include build/extended_secure_rules.mk

# Add our own (WMSDK-specific) rules on top

#---------------MFG Creation-------------------#

$(foreach e,$(b-exec-y), $(eval $(if $($(e)-mfg-cfg-y),$(call create_mfg_bin,$(e),$($(e)-dir-y)/$($(e)-mfg-cfg-y),mfg-$(e)))))

$(foreach e,$(b-exec-y), $(eval $(if $($(e)-mfg-cfg-y),$(call clean_mfg_bin_rule,$(e)))))


#---------------End of MFG creation-------------#

#--------------Upgrade image creation-----------#

define check_creation_upg_bin
ifneq ($($(1)-upg-img-y),)
ifeq ($($(1)-upg-cfg-y),)
$(1)-upg-cfg-y := fw_generator.config
endif

ifeq ($($(1)-upg-crypto-y),)
$(1)-upg-crypto-y := chacha20_ed25519
endif
endif
endef

$(foreach e,$(b-exec-y), $(eval $(call check_creation_upg_bin,$(e))))
$(foreach e,$(b-exec-y), $(eval $(call create_upg_bin,$(e),$($(e)-upg-img-y),$($(e)-dir-y)/$($(e)-upg-cfg-y),$($(e)-upg-crypto-y))))

#-----------------End of Upgrade Image Creation---------#

