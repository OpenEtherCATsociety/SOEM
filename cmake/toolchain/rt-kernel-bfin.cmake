# Licensed under the GNU General Public License version 2 with exceptions. See
# LICENSE file in the project root for full license information

# the name of the target operating system
set(CMAKE_SYSTEM_NAME rt-kernel)

# which compiler to use
set(CMAKE_C_COMPILER $ENV{COMPILERS}/bfin-elf/bin/bfin-elf-gcc)
set(CMAKE_CXX_COMPILER $ENV{COMPILERS}/bfin-elf/bin/bfin-elf-gcc)
if(CMAKE_HOST_WIN32)
  set(CMAKE_C_COMPILER ${CMAKE_C_COMPILER}.exe)
  set(CMAKE_CXX_COMPILER ${CMAKE_CXX_COMPILER}.exe)
endif(CMAKE_HOST_WIN32)

set(ARCH bfin)
set(CPU bfin)

list(APPEND MACHINE
  -mcpu=bf537
  )

add_definitions(${MACHINE})
add_link_options(${MACHINE})
