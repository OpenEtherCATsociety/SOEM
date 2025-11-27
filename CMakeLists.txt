# This software is dual-licensed under GPLv3 and a commercial
# license. See the file LICENSE.md distributed with this software for
# full license information.

cmake_minimum_required(VERSION 3.28)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
project(SOEM VERSION 2.0.0 LANGUAGES C)

# Debug enable
option(EC_DEBUG "Enable debug output")

# Configurable sizes
set(EC_BUFSIZE EC_MAXECATFRAME CACHE STRING "standard frame buffer size in bytes")
set(EC_MAXBUF 16 CACHE STRING "number of frame buffers per channel (tx, rx1 rx2)")
set(EC_MAXEEPBITMAP 128 CACHE STRING "size of EEPROM bitmap cache")
set(EC_MAXEEPBUF "EC_MAXEEPBITMAP << 5" CACHE STRING "size of EEPROM cache buffer")
set(EC_LOGGROUPOFFSET 16 CACHE STRING "default group size in 2^x")
set(EC_MAXELIST 64 CACHE STRING "max. entries in EtherCAT error list")
set(EC_MAXNAME 40 CACHE STRING "max. length of readable name in slavelist and Object Description List")
set(EC_MAXSLAVE 200 CACHE STRING "max. number of slaves in array")
set(EC_MAXGROUP 2 CACHE STRING "max. number of groups")
set(EC_MAXIOSEGMENTS 64 CACHE STRING "max. number of IO segments per group")
set(EC_MAXMBX 1486 CACHE STRING "max. mailbox size")
set(EC_MBXPOOLSIZE 32 CACHE STRING "number of mailboxes in pool")
set(EC_MAXEEPDO 0x200 CACHE STRING "max. eeprom PDO entries")
set(EC_MAXSM 8 CACHE STRING "max. SM used")
set(EC_MAXFMMU 4 CACHE STRING "max. FMMU used")
set(EC_MAXLEN_ADAPTERNAME 128 CACHE STRING "max. adapter name length")
set(EC_MAX_MAPT 1 CACHE STRING " define maximum number of concurrent threads in mapping")
set(EC_MAXODLIST 1024 CACHE STRING "max entries in Object Description list")
set(EC_MAXOELIST 256 CACHE STRING "max entries in Object Entry list")
set(EC_SOE_MAXNAME 60 CACHE STRING "max. length of readable SoE name")
set(EC_SOE_MAXMAPPING 64 CACHE STRING "max. number of SoE mappings")

# Configurable timeouts and retries
set(EC_TIMEOUTRET 2000 CACHE STRING "timeout value in us for tx frame to return to rx")
set(EC_TIMEOUTRET3 "EC_TIMEOUTRET * 3" CACHE STRING "timeout value in us for safe data transfer, max. triple retry")
set(EC_TIMEOUTSAFE 20000 CACHE STRING "timeout value in us for return \"safe\" variant (f.e. wireless)")
set(EC_TIMEOUTEEP 20000 CACHE STRING "timeout value in us for EEPROM access")
set(EC_TIMEOUTTXM 20000 CACHE STRING "timeout value in us for tx mailbox cycle")
set(EC_TIMEOUTRXM 700000 CACHE STRING "timeout value in us for rx mailbox cycle")
set(EC_TIMEOUTSTATE 2000000 CACHE STRING "timeout value in us for check statechange")
set(EC_DEFAULTRETRIES 3 CACHE STRING "default number of retries if wkc <= 0")

# MAC addresses
set(EC_PRIMARY_MAC "01:01:01:01:01:01" CACHE STRING "Primary MAC address")
set(EC_SECONDARY_MAC "04:04:04:04:04:04" CACHE STRING "Secondary MAC address")

if(PROJECT_IS_TOP_LEVEL)
  # Make option visible in ccmake, cmake-gui
  option(BUILD_SHARED_LIBS "Build shared library" OFF)
  option(SOEM_BUILD_SAMPLES "Build samples" ON)

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
  $<BUILD_INTERFACE:${SOEM_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${SOEM_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_include_directories(soem PUBLIC
  $<BUILD_INTERFACE:${SOEM_SOURCE_DIR}/osal>
  $<INSTALL_INTERFACE:include>
)

target_compile_definitions(soem PRIVATE
  $<$<BOOL:${EC_DEBUG}>:EC_DEBUG>
  $<$<STREQUAL:${CMAKE_C_BYTE_ORDER},BIG_ENDIAN>:EC_BIG_ENDIAN>
)

# Convert mac address to word arrays for use in options file
macro(convert_mac address array)
  set(RE_BYTE "([0-9A-Fa-f][0-9A-Fa-f])")
  string(REGEX REPLACE
    "^${RE_BYTE}:${RE_BYTE}:${RE_BYTE}:${RE_BYTE}:${RE_BYTE}:${RE_BYTE}$"
    "{0x\\1\\2, 0x\\3\\4, 0x\\5\\6}"
    ${array}
    ${address})
endmacro()

convert_mac(${EC_PRIMARY_MAC} EC_PRIMARY_MAC_ARRAY)
convert_mac(${EC_SECONDARY_MAC} EC_SECONDARY_MAC_ARRAY)

configure_file(
  include/soem/ec_options.h.in
  ${SOEM_BINARY_DIR}/include/soem/ec_options.h
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
  ${SOEM_BINARY_DIR}/include/soem/ec_options.h
  DESTINATION include/soem
)

install(FILES
  scripts/eniconv.py
  DESTINATION scripts
)

install(FILES
  cmake/AddENI.cmake
  DESTINATION cmake
)

install(FILES
  README.md
  LICENSE.md
  DESTINATION .
)

if(SOEM_BUILD_SAMPLES)
  add_subdirectory(samples/ec_sample)
  add_subdirectory(samples/eepromtool)
  add_subdirectory(samples/firm_update)
  add_subdirectory(samples/simple_ng)
  add_subdirectory(samples/slaveinfo)

  if (${CMAKE_SYSTEM_NAME} STREQUAL Linux)
    add_subdirectory(samples/eoe_test)
  endif()

  find_package (Python3 QUIET)
  if (Python3_FOUND)
    add_subdirectory(samples/eni_test)
  endif()
endif()

# Platform configuration
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/${CMAKE_SYSTEM_NAME}.cmake)

include (InstallRequiredSystemLibraries)
set (CPACK_RESOURCE_FILE_LICENSE "${SOEM_SOURCE_DIR}/LICENSE.md")
set (CPACK_PACKAGE_CONTACT info.soem@rt-labs.com)
set (CPACK_PACKAGE_NAME soem)
set (CPACK_SYSTEM_NAME ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR})
include (CPack)
