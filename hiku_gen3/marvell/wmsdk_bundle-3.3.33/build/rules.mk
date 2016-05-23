# Copyright (C) 2008-2016, Marvell International Ltd.
# All Rights Reserved.
#
# Description:
# -------------
# This file, rules.mk contains rules/functions for:
#
#	Object file creation from source files, where source
#	files are of following types:
#
# 	.c/.S/.s file 	--> .o file 	(b-cmd-c-to-o)
# 	.cc/.cpp file 	--> .o file 	(b-cmd-cpp-to-o)
#
# 	Directory creation 		(create_dir)
# 	Subdirectory inclusions 	(inc_mak)
# 	Library creation 		(create_lib)
# 	Axf creation			(create_prog)
# 	Obj creation 			(create_obj)

##################### Operating Variables
# The list of all the libraries in the system
b-libs-y :=
b-libs-paths-y :=
# The list of all the programs in the system
b-exec-y :=
b-exec-cpp-y :=
b-exec-apps-y :=
# The list of dependencies
b-deps-y :=
# The output directory for this configuration, this is deferred on-purpose
b-output-dir-y ?=
# The list of object directories that should be created
b-object-dir-y :=
# The list of libraries that are pre-built somewhere outside
global-prebuilt-libs-y :=
# This is path of output directory for libraries
b-libs-output-dir-y ?= $(b-output-dir-y)/libs
# This is path of output directory for intermediate files (.d,.o,.cmd)
b-objs-output-dir-y ?= $(b-output-dir-y)/objs

# This creates a rule for directory creation
# Use directories as order-only (|) prerequisites
define create_dir
$(1):
	$$(AT)$$(t_mkdir) -p $$@
endef

##################### Subdirectory Inclusions
# The wildcard, let's users pass options like
# subdir-y += mydir/Makefile.ext
#    If the Makefile has a different name, that is facilitated by the 'if block'.
#    The standard case of handling 'directory' names is facilitated by the 'else block'.
#
real-subdir-y :=
define inc_mak
 ifeq ($(wildcard $(1)/build.mk),)
     d=$(dir $(1))
     include $(1)
 else
     d=$(1)
     include $(1)/build.mk
 endif
 # For recursive subdir-y, append $(d)
 $$(foreach t,$$(subdir-y),$$(eval real-subdir-y += $$(d)/$$(t)))
 subdir-y :=
 include build/post-subdir.mk
endef

# Include rules.mk from all the subdirectories and process them
ifneq ($(subdir-y),)

# Default level subdir-y inclusion (level 1)
b-subdir-y := $(subdir-y)
subdir-y :=

# sort b-subdir-y to remove duplicates
$(foreach dir,$(sort $(b-subdir-y)),$(eval $(call inc_mak,$(dir))))

# inclusion (level 2)
b-subdir-y := $(real-subdir-y)
real-subdir-y :=
$(foreach dir,$(b-subdir-y),$(eval $(call inc_mak,$(dir))))

# inclusion (level 3)
b-subdir-y := $(real-subdir-y)
real-subdir-y :=
$(foreach dir,$(b-subdir-y),$(eval $(call inc_mak,$(dir))))
endif

# Give warning if level of subdir-recursion is greater than level 3
ifneq ($(subdir-y),)
$(warning "[Warning] You have more levels of subdir-y recursion.")
endif

include build/refine.mk

.SUFFIXES:

##################### Object (.o) Creation

# All the prog-objs-y have prog-cflags-y as the b-trgt-cflags-y variable
$(foreach l,$(b-exec-y),$(eval $($(l)-objs-y): b-trgt-cflags-y := $($(l)-cflags-y)))
# All the lib-objs-y have lib-cflags-y as the b-trgt-cflags-y
# variable. This allows configuration flags specific only to certain
# libraries/programs. Only that may be dangerous.
$(foreach l,$(b-libs-y),$(eval $($(l)-objs-y): b-trgt-cflags-y := $($(l)-cflags-y)))

# Rules for output directory creation for all the objects
$(foreach d,$(sort $(b-object-dir-y)),$(eval $(call create_dir,$(d))))

# The command for converting a .c/.cc/.cpp/.S/.s to .o
# arg1 the .c/.cc/.cpp/.S/.s filename
# arg2 the object filename
#
# This file has the default rule that maps an object file from the standard
# build output directory to the corresponding .c/.cc/.cpp/.S/.s file in the src directory
#
define b-cmd-c-to-o
  @echo " [cc] $(1)"
  $(AT)$(CC) $(b-trgt-cflags-y) $(global-cflags-y) $(global-c-cflags-y) -o $(2) -c $(1) -MMD
endef

ifneq ($(CONFIG_ENABLE_CPP_SUPPORT),)
define b-cmd-cpp-to-o
  @echo " [cpp] $@"
  $(AT)$(CPP) $(b-trgt-cflags-y) $(global-cflags-y) $(global-cpp-cflags-y) -o $(2) -c $(1) -MMD
endef
endif

# Rule for creating an object file
# strip off the $(b-output-dir-y) from the .c/.cc/.cpp/.S/.s files pathname

# Following dependency rule only checks existence of object file directory, not its timestamp
$(foreach l,$(b-libs-y) $(b-exec-y),$(foreach o,$($(l)-objs-y),$(eval $(o): | $(dir $(o)))))

define create_obj
$$(b-objs-output-dir-y)/$(1)%.o: $(2)%.c $$(b-autoconf-file)
	$$(call b-cmd-c-to-o,$$<,$$@)

$$(b-objs-output-dir-y)/$(1)%.o: $(2)%.cc $$(b-autoconf-file)
	$$(call b-cmd-cpp-to-o,$$<,$$@)

$$(b-objs-output-dir-y)/$(1)%.o: $(2)%.cpp $$(b-autoconf-file)
	$$(call b-cmd-cpp-to-o,$$<,$$@)

$$(b-objs-output-dir-y)/$(1)%.o: $(2)%.S $$(b-autoconf-file)
	$$(call b-cmd-c-to-o,$$<,$$@)

$$(b-objs-output-dir-y)/$(1)%.o: $(2)%.s $$(b-autoconf-file)
	$$(call b-cmd-c-to-o,$$<,$$@)
endef

$(eval $(call create_obj))

ifdef drive-list-y
$(foreach drv,$(drive-list-y),$(eval $(call create_obj,$(drv)$(escape_dir_name)/,$(drv)$(escape_let)/)))
endif

-include $(b-deps-y)

##################### Library (.a) Creation

# Rule for creating a library
# Given liba
#  - create $(b-output-dir-y)/liba.a
#  - from $(liba-objs-y)
#  - create a target liba.a that can be used to build the lib

$(eval $(call create_dir,$(b-libs-output-dir-y)))

define create_lib
  $(1).a: $(b-libs-output-dir-y)/$(1).a

  # Following dependency rule only checks existence of $(b-libs-output-dir-y), not its timestamp
  $(b-libs-output-dir-y)/$(1).a: | $(b-libs-output-dir-y)

  $(b-libs-output-dir-y)/$(1).a: $$($(1)-objs-y)
	$$(AT)$(t_rm) -f $$@
	@echo " [ar] $$@"
	$$(AT)$$(AR) cru $$@ $$^

  .PHONY: $(1).a.clean
  clean: $(1).a.clean
  $(1).a.clean:
	@echo " [clean] $(1)"
	$$(AT) $(t_rm) -f $(b-libs-output-dir-y)/$(1).a $$($(1)-objs-y) $$($(1)-objs-y:.o=.d)

endef

$(foreach l,$(b-libs-y), $(eval $(call create_lib,$(l))))

.PHONY: all-libs
all-libs: $(foreach l,$(b-libs-y),$(l).a)

##################### Program (.axf) Creation
# Rule for creating a program
# Given myprog
#  - create $(b-output-dir-y)/myprog
#  - from $(myprog-objs-y)
#  - add dependency on all the libraries, linker scripts
#  - create a target app_name.app which can be used to build the app

$(foreach d,$(sort $(foreach p,$(b-exec-y),$($(p)-output-dir-y))),$(eval $(call create_dir,$(d))))

define b-cmd-axf
  @echo " [axf] $(call b-abspath,$(2))"
  $(AT)$($(1)-LD) -o $(2) $($(1)-objs-y) $($(1)-lflags-y) $($(1)-cflags-y) $(start-group-opt) $($(1)-prebuilt-libs-y) $($(1)-libs-paths-y) $(global-prebuilt-libs-y) $(end-group-opt) $(ld-opt) $($(1)-linkerscript-y) $(map-opt) $(2:%.axf=%.map) $(toolchain-linkerscript-flags-y) $(LDFLAGS-y) $(global-cflags-y)
endef

define b-cmd-disppath-mapfile
  @echo " [map] $(call b-abspath,$(1:%.axf=%.map))"
endef

define create_prog

# $(1)-dir-y is created in build/post-subdir.mk
# by the name $(l)-dir-y

  %/$(subst $(escape_let),$(escape_dir_name),$($(1)-dir-y))/$$(notdir $${$(1)-board-y:.c=.o}): $$($(1)-board-y) $(b-autoconf-file)
	$$(call b-cmd-c-to-o,$$<,$$@)

# $(1)-output-dir-y is  $(b-output-dir-y)/<board-name>

  $(1).app: $($(1)-output-dir-y)/$(1).axf
  $(1).axf: $($(1)-output-dir-y)/$(1).axf

# Following dependency rule only checks existence of $(1)-output-dir-y, not its timestamp
  $($(1)-output-dir-y)/$(1).axf: | $($(1)-output-dir-y)

  $($(1)-output-dir-y)/$(1).axf: $$($(1)-objs-y) $$($(1)-libs-paths-y) $$($(1)-linkerscript-y) $$(global-prebuilt-libs-y)
	$$(call b-cmd-axf,$(1),$$@)
	$$(call b-cmd-disppath-mapfile,$$@)

  .PHONY: $(1).app.clean
  clean: $(1).app.clean
  $(1).app.clean:
	@echo " [clean] $(1)"
	$$(AT)$(t_rm) -f $($(1)-output-dir-y)/$(1).axf $($(1)-output-dir-y)/$(1).map $$($(1)-objs-y) $$($(1)-objs-y:.o=.d)
endef

$(foreach p,$(b-exec-y), $(eval $(call create_prog,$(p))))

# Include app_postprocess function from app makefile
$(foreach p,$(b-exec-y), $(eval $(call $(p)_postprocess)))

##################### Miscellaneous handling

# Rule for clean
#
clean:

# Rule for NOISY Output
ifneq ($(NOISY),1)
AT=@
endif

FORCE:
