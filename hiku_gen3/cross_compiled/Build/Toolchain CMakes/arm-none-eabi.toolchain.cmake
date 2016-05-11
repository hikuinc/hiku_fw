set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_AR $ENV{HOME}/arm-none-eabi/bin/arm-none-eabi-ar CACHE PATH "ARM archiver")

find_program(CMAKE_C_COMPILER $ENV{HOME}/arm-none-eabi/bin/arm-none-eabi-gcc)
find_program(CMAKE_CXX_COMPILER $ENV{HOME}/arm-none-eabi/bin/arm-none-eabi-g++)

set(CMAKE_SHARED_LINKER_FLAGS "--specs=nosys.specs" CACHE STRING "Shared linker flags")
set(CMAKE_MODULE_LINKER_FLAGS "--specs=nosys.specs" CACHE STRING "Module linker flags")
set(CMAKE_EXE_LINKER_FLAGS    "--specs=nosys.specs" CACHE STRING "Executable linker flags")

set(CMAKE_FIND_ROOT_PATH $ENV{HOME}/arm-none-eabi/arm-none-eabi)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
