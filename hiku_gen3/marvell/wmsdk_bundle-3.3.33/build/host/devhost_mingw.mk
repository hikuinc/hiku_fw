# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

os_dir := Windows
file_ext := .exe

t_mconf = mconf_not_on_mingw
mconf_not_on_mingw:
	@echo ""
	@echo "The 'menuconfig' option is not supported in MinGW"
	@echo "Please use 'make config' instead"
	@echo ""
	@false
