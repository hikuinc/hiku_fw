
.PHONY: $(t_flashprog)


t_flashprog := $(d)/flashprog.axf

flashprog-install-dir := $(dir $(t_flashprog))/../../OpenOCD/$(arch_name-y)

flashprog-deps-list-y := libpart.a libftfs.a libdrv.a

$(t_flashprog): $(foreach l,$(flashprog-deps-list-y),$(b-libs-output-dir-y)/$(l))
	@echo "[$(notdir $@)]"
	$(AT)$(MAKE) -s -C $(dir $@) SDK_DIR=$(call b-abspath,$(CURDIR)) LIBS_PATH=$(b-libs-output-dir-y) TARGET=$(notdir $@)
	@echo " [install] $(notdir $@)"
	$(AT)$(t_cp) -a  $@  $(flashprog-install-dir)/

$(t_flashprog).clean:
	@echo " [clean] $(notdir $(t_flashprog))"
	$(AT)$(MAKE) -s -C $(dir $@) SDK_DIR=$(call b-abspath,$(CURDIR)) LIBS_PATH=$(b-libs-output-dir-y) TARGET=$(notdir $(t_flashprog)) clean

tools.install: $(t_flashprog)

tools.clean: $(t_flashprog).clean
