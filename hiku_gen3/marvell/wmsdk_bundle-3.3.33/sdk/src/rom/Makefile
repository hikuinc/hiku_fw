# Copyright (C) 2008-2014, Marvell International Ltd.
# All Rights Reserved.

# Current ROM version, can be over-ridden
SDK_ROM_MAJOR ?= 0
SDK_ROM_MINOR ?= 4
VER := $(SDK_ROM_MAJOR).$(SDK_ROM_MINOR)

LIB := libinrom.a

all: rom_syms headers Makefile

rom_syms:
	$(COPY_CMD) -f -a -L $(VER)/$(LIB) ./

headers:
	@mkdir -p incl/
	$(COPY_CMD) -f -a -L $(VER)/ctaocrypt/cyassl incl/
	$(COPY_CMD) -f -a -L $(VER)/allocator incl/
	$(COPY_CMD) -f -a -L $(VER)/ver incl/
	$(COPY_CMD) -f -a -L $(VER)/xz incl/
	$(COPY_CMD) -f -a -L $(VER)/libc incl/
	$(COPY_CMD) -f -a -L $(VER)/poly1305 incl/
	$(COPY_CMD) -f -a -L $(VER)/chacha20 incl/

clean :
	@-rm -f *.a
	@-rm -rf incl/
