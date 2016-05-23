
# Handle boot2 version
BOOT2_VERSION_INTERNAL=1.1
BOOT2_VERSION=\"$(BOOT2_VERSION_INTERNAL)$(BOOT2_EXTRA_VERSION)\"

ifneq ($(JTAG_LOCK_PASSWORD),)
	lock := -locked
endif
prj_bldr := boot2$(lock)
boot2_name := boot2$(lock)

b-axf-only += $(prj_bldr)

exec-y += $(prj_bldr)

$(prj_bldr)-objs-y += \
	main.c \
	boot2.c \
	../utils/crc32.c

# Compiler flags definition.
$(prj_bldr)-cflags-y := \
		-ggdb \
		-I $(d) \
		-DBOOT2_VERSION=$(BOOT2_VERSION) \
		-DFREERTOS


$(prj_bldr)-select-libs-y += libdrv.a

$(prj_bldr)-linkerscript-y := $(d)/boot2.ld

# This post-processes boot2.axf in a different way than default method

define $(prj_bldr)_postprocess
boot2: $($(prj_bldr)-output-dir-y)/$(boot2_name).bin

$($(prj_bldr)-output-dir-y)/$(boot2_name).bin: $($(prj_bldr)-output-dir-y)/$(prj_bldr).axf $$(t_bh_generator)
	$$(AT)$$(OBJCOPY) $$< -O binary $($(prj_bldr)-output-dir-y)/bootloader.bin
	$$(AT)$$(t_bh_generator) $($(prj_bldr)-output-dir-y)/bootloader.bin $($(prj_bldr)-output-dir-y)/bootheader.bin
	$$(AT)$$(t_cat) $($(prj_bldr)-output-dir-y)/bootheader.bin $($(prj_bldr)-output-dir-y)/bootloader.bin > $$@
	$$(AT)$$(t_rm) -f $($(prj_bldr)-output-dir-y)/bootloader.bin
	$$(AT)$$(t_rm) -f $($(prj_bldr)-output-dir-y)/bootheader.bin
	@echo " [bin] $$(abspath $$@)"

boot2.clean: $(prj_bldr).app.clean
$(prj_bldr).app.clean: boot2.app.clean.bin
boot2.app.clean.bin:
	$$(AT)$$(t_rm) -f $($(prj_bldr)-output-dir-y)/$(boot2_name).bin
endef
