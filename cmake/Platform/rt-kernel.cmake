#********************************************************************
#        _       _         _
#  _ __ | |_  _ | |  __ _ | |__   ___
# | '__|| __|(_)| | / _` || '_ \ / __|
# | |   | |_  _ | || (_| || |_) |\__ \
# |_|    \__|(_)|_| \__,_||_.__/ |___/
#
# www.rt-labs.com
# Copyright 2017 rt-labs AB, Sweden.
#
# This software is licensed under the terms of the BSD 3-clause
# license. See the file LICENSE distributed with this software for
# full license information.
#*******************************************************************/

include_guard()
cmake_minimum_required (VERSION 3.16)

# Avoid warning when re-running cmake
set(DUMMY ${CMAKE_TOOLCHAIN_FILE})

# No support for shared libs
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)

set(UNIX 1)
set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")
set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# Do not build executables during configuration stage. Required
# libraries may not be built yet.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Prefer standard extensions
set(CMAKE_C_OUTPUT_EXTENSION_REPLACE 1)
set(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE 1)
set(CMAKE_ASM_OUTPUT_EXTENSION_REPLACE 1)

# Add machine-specific flags
add_compile_options(${MACHINE})
add_link_options(${MACHINE})

if (NOT ${VARIANT} STREQUAL "")
  add_definitions(-D${VARIANT})
endif()

# Common flags
add_compile_options(
  -fdata-sections
  -ffunction-sections
  -fomit-frame-pointer
  -fno-strict-aliasing
  -fshort-wchar
  )
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")

# Linker script
set(TYPE "" CACHE STRING "BSP link type")
if (NOT ${TYPE} STREQUAL "")
  set(_TYPE "_${TYPE}")
endif()

define_property(TARGET
  PROPERTY BSP_LINK_SCRIPT INHERITED
  BRIEF_DOCS "Define the linker script to use when linking executable"
  FULL_DOCS  "Define the linker script to use when linking executable"
  )

set_directory_properties(
  PROPERTIES BSP_LINK_SCRIPT ${BSP_DIR}/${BSP}${_TYPE}.ld
  )

# Linker flags
add_link_options(
  -nostartfiles
  -T$<TARGET_PROPERTY:BSP_LINK_SCRIPT>
  -Wl,--gc-sections
  )

# Standard libraries
link_libraries(
  c
  m
  $<$<LINK_LANGUAGE:CXX>:-lstdc++>
  )

# Group libraries when linking
set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> -Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group -Wl,-Map=<TARGET>.map,--cref")

set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> -Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group -Wl,-Map=<TARGET>.map,--cref")

# Macro to add TTOS schedule
macro(target_tt_schedule target infile)
  find_package(Python3 REQUIRED)
  string(REPLACE .tt .c outfile ${infile})
  add_custom_command(
    OUTPUT ${outfile}
    COMMAND  ${Python3_EXECUTABLE} ${TT_PARSE} ${CMAKE_CURRENT_LIST_DIR}/${infile} > ${outfile}
    DEPENDS ${infile}
    )
  target_sources(${target} PRIVATE ${outfile})
endmacro()
