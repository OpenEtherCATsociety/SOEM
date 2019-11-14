# Licensed under the GNU General Public License version 2 with exceptions. See
# LICENSE file in the project root for full license information

# Guard against multiple inclusion
if(_RT_KERNEL_IMX6_CMAKE_)
  return()
endif()
set(_RT_KERNEL_IMX6_CMAKE_ TRUE)

# the name of the target operating system
set(CMAKE_SYSTEM_NAME rt-kernel)

# which compiler to use
set(CMAKE_C_COMPILER $ENV{COMPILERS}/arm-eabi/bin/arm-eabi-gcc)
set(CMAKE_CXX_COMPILER $ENV{COMPILERS}/arm-eabi/bin/arm-eabi-gcc)
if(CMAKE_HOST_WIN32)
  set(CMAKE_C_COMPILER ${CMAKE_C_COMPILER}.exe)
  set(CMAKE_CXX_COMPILER ${CMAKE_CXX_COMPILER}.exe)
endif(CMAKE_HOST_WIN32)

set(ARCH imx6)
set(CPU cortex-a9-vfp)

list(APPEND MACHINE
  -mcpu=cortex-a9
  -mfpu=vfpv3-d16
  -mfloat-abi=hard
  )

add_definitions(${MACHINE})
add_link_options(${MACHINE})
