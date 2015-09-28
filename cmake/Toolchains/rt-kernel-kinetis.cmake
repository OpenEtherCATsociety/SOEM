
message("rt-kernel-kinetis.cmake")

set(CMAKE_SYSTEM_NAME rt-kernel)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR kinetis)

set(ARCH kinetis CACHE STRING "Architecture")
set(BSP twrk60 CACHE STRING "Board")
