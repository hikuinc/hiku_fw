t_fw_generator := $(d)/fw_generator$(file_ext)
crypto_opt := chacha20_ed25519


# "%" stands for Crypto option to be passed by user

$(t_fw_generator).%.make:
	$(eval trgt := $*_$(notdir $(t_fw_generator)))
	$(AT)$(MAKE) -s -C $(dir $@) SDK_DIR=$(call b-abspath,$(CURDIR)) CC=$(HOST_CC) TARGET=$(trgt) CRYTO_OPTION=$* all

$(t_fw_generator).%.clean:
	$(eval trgt := $*_$(notdir $(t_fw_generator)))
	@echo " [clean] $(trgt)"
	$(AT)$(MAKE) -s -C $(dir $@) SDK_DIR=$(call b-abspath,$(CURDIR)) CC=$(HOST_CC) TARGET=$(trgt) CRYTO_OPTION=$* clean

tools.install: $(t_fw_generator).$(crypto_opt).make

tools.clean: $(t_fw_generator).$(crypto_opt).clean

##################### UPG (.upg) Creation
# Input Parameters expected:
# 	1. <app_name> e.g. hello_world
# 	2. <desired_upg_name> e.g. hello_world.upg
# 	3. <config_file_with_relative_path_to_SDK>
# 	   e.g. hello_world.config (where file resides in hello_world folder)
# 	4. <crypto_config_option> e.g. chacha20_ed25519 for building a flavor of
# 	   fw_generator tools
#
# How to call function create_upg_bin
# 	$(call create_upg_bin,hello_world,hello_world.upg,/path/to/config_file/relative/to/SDK,chacha20_ed25519)
#

define create_upg_bin

ifneq ($(2),)

ifneq ($(notdir $(3)),)

ifneq ($(wildcard $(3)),)

ifneq ($(4),)

$(1).app: $($(1)-output-dir-y)/$(2)

$($(1)-output-dir-y)/$(2): $($(1)-output-dir-y)/$(1)$(sec_mcufw_type).bin

$($(1)-output-dir-y)/$(2): $(t_fw_generator).$(4).make $(3)
	$$(AT)$(dir $(t_fw_generator))$(4)_$(notdir $(t_fw_generator)) $($(1)-output-dir-y)/$(1)$(sec_mcufw_type).bin $$@ $(3) -s
	@echo " [upg] $$(abspath $$@)"

.PHONY: $(1).app.clean_upg
$(1).app.clean: $(1).app.clean_upg

$(1).app.clean_upg:
	$$(AT)$$(t_rm) -f  $($(1)-output-dir-y)/$(2)

endif
endif
endif
endif
endef


