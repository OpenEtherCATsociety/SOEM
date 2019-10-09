# Licensed under the GNU General Public License version 2 with exceptions. See
# LICENSE file in the project root for full license information

# Guard against multiple inclusion
if(_RT_KERNEL_CMAKE_)
  return()
endif()
set(_RT_KERNEL_CMAKE_ TRUE)

cmake_minimum_required (VERSION 3.1.2)

# Avoid warning when re-running cmake
set(DUMMY ${CMAKE_TOOLCHAIN_FILE})

# No support for shared libs
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)

set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")
set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# Get environment variables
set(RTK $ENV{RTK} CACHE STRING
  "Location of rt-kernel tree")
set(COMPILERS $ENV{COMPILERS} CACHE STRING
  "Location of compiler toolchain")
set(BSP $ENV{BSP} CACHE STRING
  "The name of the BSP to build for")

# Common flags
add_definitions(-fomit-frame-pointer -fno-strict-aliasing -fshort-wchar)

# Common includes
include_directories(
  $ENV{RTK}/include
  $ENV{RTK}/include/kern
  $ENV{RTK}/include/drivers
  $ENV{RTK}/include/arch/${ARCH}
  $ENV{RTK}/bsp/${BSP}/include
  $ENV{RTK}/lwip/src/include
  )

link_libraries(
  shell
  dev
  block
  fs
  usb
  lwip
  eth
  i2c
  rtc
  can
  nand
  spi
  nor
  pwm
  adc
  trace
  sio
  kern
  ${ARCH}
  ${BSP}
  c
  m
  )

set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> -nostartfiles -L${RTK}/lib/${ARCH}/${VARIANT}/${CPU} -T${RTK}/bsp/${BSP}/${BSP}.ld -Wl,--gc-sections -Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group")

set(CMAKE_C_LINK_EXECUTABLE   "<CMAKE_C_COMPILER>   <FLAGS> <CMAKE_C_LINK_FLAGS>   <LINK_FLAGS> <OBJECTS> -o <TARGET> -nostartfiles -L${RTK}/lib/${ARCH}/${VARIANT}/${CPU} -T${RTK}/bsp/${BSP}/${BSP}.ld -Wl,--gc-sections -Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group")
