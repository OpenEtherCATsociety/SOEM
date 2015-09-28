
message("rt-kernel-gcc.cmake")

set(CMAKE_C_OUTPUT_EXTENSION .o)
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)

set(CFLAGS "${CFLAGS} -Wall -Wextra -Wno-unused-parameter -Werror")
set(CFLAGS "${CFLAGS} -fomit-frame-pointer -fno-strict-aliasing -fshort-wchar")
#set(CFLAGS" ${CFLAGS} -B$(GCC_PATH)/libexec/gcc")

set(CXXFLAGS "${CXXFLAGS} -fno-rtti -fno-exceptions")

set(LDFLAGS "${LDFLAGS} -nostartfiles")

set(CMAKE_C_FLAGS "${CFLAGS} ${MACHINE}" CACHE STRING "")
set(CMAKE_EXE_LINKER_FLAGS "${MACHINE} ${LDFLAGS}" CACHE STRING "")



