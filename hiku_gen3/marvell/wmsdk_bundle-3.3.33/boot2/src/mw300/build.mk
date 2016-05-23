
# Handle boot2 version
BOOT2_VERSION_INTERNAL=1.1
BOOT2_VERSION=\"$(BOOT2_VERSION_INTERNAL)$(BOOT2_EXTRA_VERSION)\"

path_to_libinrom := $(d)/../../../sdk/src/rom/0.4

prj_bldr := boot2
boot2_name := boot2
exec-y += $(prj_bldr)

b-axf-only += $(prj_bldr)

$(prj_bldr)-objs-y += \
	main.c \
	boot2.c \
	../utils/crc32.c \
	aes.c \
	secure_boot2.c

# Compiler flags definition.
$(prj_bldr)-cflags-y := \
		-ggdb \
		-I $(d) \
		-DBOOT2_VERSION=$(BOOT2_VERSION) \
		-DFREERTOS

# ROM lib and headers
$(prj_bldr)-cflags-y += -I $(path_to_libinrom)/ctaocrypt/

#UART_DEBUG=y

$(prj_bldr)-cflags-$(UART_DEBUG) += -DUART_DEBUG

$(prj_bldr)-select-libs-y += libdrv.a



$(prj_bldr)-prebuilt-libs-y := $(path_to_libinrom)/libinrom.a
$(prj_bldr)-linkerscript-y := $(d)/boot2.ld

# This post-processes boot2.axf in a different way than default method

define $(prj_bldr)_postprocess
ifeq ($(b-secboot-y),)

# Non-secure boot2 (.bin) creation
boot2: $($(prj_bldr)-output-dir-y)/$(boot2_name).bin

$($(prj_bldr)-output-dir-y)/$(boot2_name).bin: $($(prj_bldr)-output-dir-y)/$(prj_bldr).axf $$(t_bh_generator)
	$$(AT)$$(t_rm) -f $$(wildcard $($(prj_bldr)-output-dir-y)/$(boot2_name)*.bin)
	$$(AT)$$(OBJCOPY) $$< -O binary $($(prj_bldr)-output-dir-y)/bootloader.bin
	$$(AT)$$(t_bh_generator) $($(prj_bldr)-output-dir-y)/bootloader.bin $$@
	$$(AT)$$(t_rm) -f $($(prj_bldr)-output-dir-y)/bootloader.bin
	@echo " [bin] $$(abspath $$@)"
else

# Secure boot2 ([.][k][e][s].bin) creation
boot2: $($(prj_bldr)-output-dir-y)/$(boot2_name)$(sec_boot2_type).bin

$($(prj_bldr)-output-dir-y)/$(boot2_name)$(sec_boot2_type).bin: $($(prj_bldr)-output-dir-y)/$(prj_bldr).axf $$(t_bh_generator) $$(sec_conf) $$(ks_hdr) $$(t_secboot)
	$$(AT)$$(t_rm) -f $$(wildcard $($(prj_bldr)-output-dir-y)/$(boot2_name)*.bin)
	$$(AT)$$(OBJCOPY) $$< -O binary $($(prj_bldr)-output-dir-y)/bootloader.bin
	$$(AT)$$(t_bh_generator) $($(prj_bldr)-output-dir-y)/bootloader.bin $($(prj_bldr)-output-dir-y)/$(boot2_name).bin
ifneq ($$(sec_boot2_type),)
	$$(AT)$$(t_python) $$(t_secboot) -c $$(sec_conf) -s $$(ks_hdr) -b $($(prj_bldr)-output-dir-y)/$(boot2_name).bin $$(secboot_flags)
	$$(AT)$$(t_rm) -f $($(prj_bldr)-output-dir-y)/$(boot2_name).bin
endif
	$$(AT)$$(t_rm) -f $($(prj_bldr)-output-dir-y)/bootloader.bin
	@echo " [sbin] $$(abspath $$@)"
endif

# Common to both secure and non-secure build
boot2.clean: $(prj_bldr).app.clean
$(prj_bldr).app.clean: boot2.app.clean.bin
boot2.app.clean.bin:
	$$(AT)$$(t_rm) -f $$(wildcard $($(prj_bldr)-output-dir-y)/$(boot2_name)*.bin)
endef
