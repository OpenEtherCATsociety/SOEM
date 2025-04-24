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

#[=======================================================================[.rst:
rt-kernel toolchain
-------------------

The following variables must be set when cmake configuration is
invoked::

  BSP           - Name of Board Support Package

The following variables are optional::

  COMPILERS - Compiler search path, defaults to /opt/rt-tools/compilers
  RTK       - Location of rt-kernel tree, defaults to root of toolchain file
  BSP_DIR   - Location of BSP directory, defaults to ${RTK}/bsp/${BSP}
  TT_PARSE  - Location of tt_parse, defaults to ${RTK}/script/tt_parse.py

The variables can set from environment variables or from CMake
configuration. Your CMAKE_MODULE_PATH must also be configured so that
Platform/rt-kernel.cmake can be found.

Example to build for the xmc48relax board::

  BSP=xmc48relax cmake \
     -B build.xmc48relax \
     -S /path/to/rt-kernel \
     -DCMAKE_TOOLCHAIN_FILE=/path/to/rt-kernel.cmake \
     -G "Unix Makefiles"

The variables can also be set from the root CMakeLists.txt. Example to
build using an out-of-tree BSP::

  cmake_minimum_required(VERSION 3.16)
  set(ENV{BSP} mybsp)
  set(ENV{BSP_DIR} "${CMAKE_CURRENT_LIST_DIR}/mybsp")
  set(ENV{RTK} "${CMAKE_CURRENT_LIST_DIR}/rt-kernel")
  list(APPEND CMAKE_MODULE_PATH "$ENV{RTK}/cmake")
  project (MYPROJECT VERSION 2.0.1)

#]=======================================================================]

include_guard()
cmake_minimum_required (VERSION 3.16)

# The name of the target operating system
set(CMAKE_SYSTEM_NAME rt-kernel)

# Default toolchain search path
if (NOT DEFINED ENV{COMPILERS})
  set(ENV{COMPILERS} "/opt/rt-tools/compilers")
endif()
set(COMPILERS $ENV{COMPILERS} CACHE PATH
  "Location of compiler toolchain")

# Default rt-kernel location
if(NOT DEFINED ENV{RTK})
  # Set default to grand-parent of current dir
  get_filename_component(RTK_ROOT
    ${CMAKE_CURRENT_LIST_DIR}/../..
    ABSOLUTE
    )
  set(ENV{RTK} ${RTK_ROOT})
endif()
set(RTK $ENV{RTK} CACHE PATH
  "Location of rt-kernel tree")

# Name of BSP
set(BSP $ENV{BSP} CACHE STRING
  "The name of the BSP to build for")

# Default BSP dir
if(NOT DEFINED ENV{BSP_DIR})
  set(ENV{BSP_DIR} ${RTK}/bsp/${BSP})
endif()
set(BSP_DIR $ENV{BSP_DIR} CACHE PATH
  "Location of BSP")

# Default tt_parse search path
if (NOT DEFINED ENV{TT_PARSE})
  set(ENV{TT_PARSE} "${RTK}/script/tt_parse.py")
endif()
set(TT_PARSE $ENV{TT_PARSE} CACHE FILEPATH
  "Location of tt_parse")

message(STATUS "BSP: ${BSP}")
message(STATUS "BSP_DIR: ${BSP_DIR}")
message(STATUS "RTK: ${RTK}")

# Variables required for cmake try_compile phase
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
  "BSP"
  "BSP_DIR"
  )

# Check that BSP is set
if (NOT BSP)
  message(FATAL_ERROR "BSP has not been set")
endif()

# Check that bsp.mk exists
set(BSP_MK_FILE ${BSP_DIR}/${BSP}.mk)
if (NOT EXISTS ${BSP_MK_FILE})
  message(FATAL_ERROR "Failed to open ${BSP_MK_FILE}")
endif()

# Slurp bsp.mk contents
file(READ ${BSP_MK_FILE} BSP_MK)

# Get CPU
string(REGEX MATCH "CPU=([A-Za-z0-9_\-]*)" _ ${BSP_MK})
set(CPU ${CMAKE_MATCH_1})

# Get ARCH
string(REGEX MATCH "ARCH=([A-Za-z0-9_\-]*)" _ ${BSP_MK})
set(ARCH ${CMAKE_MATCH_1})

# Get VARIANT
string(REGEX MATCH "VARIANT=([A-Za-z0-9_\-]*)" _ ${BSP_MK})
set(VARIANT ${CMAKE_MATCH_1})

# Get CROSS_GCC
string(REGEX MATCH "CROSS_GCC=([A-Za-z0-9_\-]*)" _ ${BSP_MK})
set(CROSS_GCC ${CMAKE_MATCH_1})

# Set cross-compiler toolchain
set(CMAKE_C_COMPILER ${COMPILERS}/${CROSS_GCC}/bin/${CROSS_GCC}-gcc)
set(CMAKE_CXX_COMPILER ${CMAKE_C_COMPILER})
if(CMAKE_HOST_WIN32)
  set(CMAKE_C_COMPILER ${CMAKE_C_COMPILER}.exe)
  set(CMAKE_CXX_COMPILER ${CMAKE_CXX_COMPILER}.exe)
endif(CMAKE_HOST_WIN32)

# Set cross-compiler machine-specific flags
include(${CMAKE_CURRENT_LIST_DIR}/${CPU}.cmake)
