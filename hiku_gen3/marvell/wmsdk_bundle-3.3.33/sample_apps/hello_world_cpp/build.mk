# Copyright (C) 2008-2016 Marvell International Ltd.
# All Rights Reserved.

exec-cpp-y += hello_world_cpp
hello_world_cpp-objs-y := src/main.cc src/wmsdk-cpp-helper.cc
# Applications could also define custom linker files if required using following:
#hello_world-linkerscript-y := /path/to/linkerscript

# Applications could also define custom board files if required using following:
#hello_world-board-y := /path/to/boardfile
