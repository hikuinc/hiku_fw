set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_AR /usr/bin/i686-w64-mingw32-ar CACHE PATH "MinGW archiver")

find_program(CMAKE_C_COMPILER /usr/bin/i686-w64-mingw32-gcc)
find_program(CMAKE_CXX_COMPILER /usr/bin/i686-w64-mingw32-g++)
find_program(CMAKE_RC_COMPILER /usr/bin/i686-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
