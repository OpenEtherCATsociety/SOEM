# This software is dual-licensed under GPLv3 and a commercial
# license. See the file LICENSE.md distributed with this software for
# full license information.

cmake_minimum_required(VERSION 3.28)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
project(SOEM VERSION 2.0.0 LANGUAGES C)

if(PROJECT_IS_TOP_LEVEL)
  # Make option visible in ccmake, cmake-gui
  option(BUILD_SHARED_LIBS "Build shared library" OFF)
  option(SOEM_BUILD_UTILS "Build utitilities" ON)

  # Default to release build with debug info
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
  endif(NOT CMAKE_BUILD_TYPE)

  # Default to installing in build directory
  if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/install
      CACHE PATH "Default install path" FORCE)
  endif()

  message(STATUS "Current build type is: ${CMAKE_BUILD_TYPE}")
  message(STATUS "Current install path is: ${CMAKE_INSTALL_PREFIX}")
  message(STATUS "Building for ${CMAKE_SYSTEM_NAME}")
endif()

# Always use standard .o suffix
set(CMAKE_C_OUTPUT_EXTENSION_REPLACE 1)
set(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE 1)

add_library(soem
  src/ec_base.c
  src/ec_coe.c
  src/ec_config.c
  src/ec_dc.c
  src/ec_eoe.c
  src/ec_foe.c
  src/ec_main.c
  src/ec_print.c
  src/ec_soe.c
)

target_include_directories(soem PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_include_directories(soem PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/osal>
  $<INSTALL_INTERFACE:include>
)

install(
  TARGETS soem
  EXPORT soemConfig
  DESTINATION lib
)

install(
  EXPORT soemConfig
  DESTINATION cmake
)

install(FILES
  include/soem/ec_base.h
  include/soem/ec_coe.h
  include/soem/ec_config.h
  include/soem/ec_dc.h
  include/soem/ec_eoe.h
  include/soem/ec_foe.h
  include/soem/ec_main.h
  include/soem/ec_print.h
  include/soem/ec_soe.h
  include/soem/ec_type.h
  include/soem/soem.h
  osal/osal.h
  DESTINATION include/soem
)

if(SOEM_BUILD_UTILS)
  add_subdirectory(samples/simple_ng)
  add_subdirectory(samples/slaveinfo)
  add_subdirectory(samples/eepromtool)
  add_subdirectory(samples/simple_test)
endif()

# Platform configuration
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/${CMAKE_SYSTEM_NAME}.cmake)

include (InstallRequiredSystemLibraries)
set (CPACK_RESOURCE_FILE_LICENSE "${SOEM_SOURCE_DIR}/LICENSE.md")
set (CPACK_PACKAGE_CONTACT info.soem@rt-labs.com)
set (CPACK_PACKAGE_NAME soem)
set (CPACK_SYSTEM_NAME ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR})
include (CPack)
