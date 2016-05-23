
t_psm-create-part := $(d)/psm-create-part$(file_ext)

ifneq ($(SECURE_BOOT),)
t_psm-create-part := $(d)/psm-create-part-secure$(file_ext)
endif


ifeq ($(CONFIG_CPU_MW300), y)
ifneq ($(wildcard $(sec_conf)),)
SECURE_PSM_KEY := $(shell cat $(sec_conf) | grep "^KEY_PSM_ENCRYPT_KEY" | awk '{print $$3}')
SECURE_PSM_NONCE := $(shell cat $(sec_conf) | grep "^KEY_PSM_NONCE" | awk '{print $$3}')
endif
endif

$(t_psm-create-part):
	@echo "[$(notdir $@)]"
	$(AT)$(MAKE) -s -C $(dir $@) SDK_DIR=$(CURDIR) CC=$(HOST_CC) TARGET=$(notdir $@) SECURE_BOOT=$(SECURE_BOOT)


$(t_psm-create-part).clean:
	@echo " [clean] $(notdir $(t_psm-create-part))"
	$(AT)$(MAKE) -s -C $(dir $@) SDK_DIR=$(CURDIR) TARGET=$(notdir $(t_psm-create-part)) clean

tools.install: $(t_psm-create-part)

tools.clean: $(t_psm-create-part).clean

##################### MFG (mfg.bin) Creation
# Input Parameters expected:
# 	1. <app_name> e.g. hello_world
# 	2. <mfg_config_file_path_relative_path_to_SDK>
# 	3. <desired_mfg_bin_name> e.g. hello_world-mfg
# 		It will generate hello_world-mfg.bin

define create_mfg_bin

# This is done to handle $(1).app creating multiple mfg binaries.
# $(1)-mfg-bin-list-y variable used to collect mfg binaries for $(1).app

# Initially assigning to empty
$(1)-mfg-bin-list-y ?=
ifneq ($(notdir $(2)),)

ifeq ($(SECURE_BOOT),)
$(1).app: $($(1)-output-dir-y)/$(3).bin

$($(1)-output-dir-y)/$(3).bin: $(t_psm-create-part) $(2)
	$$(AT)$$(t_psm-create-part) $(2) $$@ -s
	@echo " [mfg] $$(abspath $$@)"

$(1)-mfg-bin-list-y += $($(1)-output-dir-y)/$(3).bin

else

$(1).app: $($(1)-output-dir-y)/$(3)-secure.bin

$($(1)-output-dir-y)/$(3)-secure.bin: $(t_psm-create-part) $(2)
	$$(AT)$$(t_psm-create-part) $(2) $$@ $(SECURE_PSM_KEY) $(SECURE_PSM_NONCE) -s
	@echo " [mfg] $$(abspath $$@)"

$(1)-mfg-bin-list-y += $($(1)-output-dir-y)/$(3)-secure.bin
endif

endif
endef

#-----------------Cleaning mfg bin files-------------#
define clean_mfg_bin_rule
.PHONY: $(1).app.clean_mfg_bin
$(1).app.clean: $(1).app.clean_mfg_bin

$(1).app.clean_mfg_bin:
	$$(AT)$$(t_rm) -f $($(1)-mfg-bin-list-y)
endef

#-----------------End of Cleaning mfg bin files-------------#
