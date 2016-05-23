# Copyright (C) 2008-2016 Marvell International Ltd.
# All Rights Reserved.

otp_prog_dir := $(d)

exec-y += otp_prog
b-axf-only += otp_prog

otp_prog-objs-y := src/main.c
otp_prog-cflags-y := -I $(otp_prog_dir)/src -DAPP_VERSION=\"0.4\"
otp_prog-lflags-y := -s
otp_prog-linkerscript-y := $(otp_prog_dir)/mw300_otp_prog.ld

$(otp_prog_dir)/src/secure_config.h: $(sec_conf) $(ks_hdr) $(t_secboot)
	$(AT)$(t_python) $(t_secboot) -c $(sec_conf) -s $(ks_hdr) -o $@ $(secboot_flags)

# Applications could also define custom board files if required using following:
#otp_prog-board-y := /path/to/boardfile
