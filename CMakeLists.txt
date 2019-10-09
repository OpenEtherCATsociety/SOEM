# Licensed under the GNU General Public License version 2 with exceptions. See
# LICENSE file in the project root for full license information

cmake_minimum_required(VERSION 3.12)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
project(SOEM C)

# Default to installing in SOEM source directory
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install
    CACHE PATH "Default install path" FORCE)
endif()

# Let platforms disable test programs
set(BUILD_TESTS TRUE)

# Always use standard .o suffix
set(CMAKE_C_OUTPUT_EXTENSION_REPLACE 1)
set(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE 1)

add_library(soem
  soem/ethercatbase.c
  soem/ethercatbase.h
  soem/ethercatcoe.c
  soem/ethercatcoe.h
  soem/ethercatconfig.c
  soem/ethercatconfig.h
  soem/ethercatconfiglist.h
  soem/ethercatdc.c
  soem/ethercatdc.h
  soem/ethercateoe.c
  soem/ethercateoe.h
  soem/ethercatfoe.c
  soem/ethercatfoe.h
  soem/ethercat.h
  soem/ethercatmain.c
  soem/ethercatmain.h
  soem/ethercatprint.c
  soem/ethercatprint.h
  soem/ethercatsoe.c
  soem/ethercatsoe.h
  soem/ethercattype.h
  )

target_include_directories(soem PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/soem>
  $<INSTALL_INTERFACE:include/soem>
  )
target_include_directories(soem PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/osal>
  $<INSTALL_INTERFACE:include/soem>
  )

include(${CMAKE_SYSTEM_NAME})

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
  soem/ethercatbase.h
  soem/ethercatcoe.h
  soem/ethercatconfig.h
  soem/ethercatconfiglist.h
  soem/ethercatdc.h
  soem/ethercateoe.h
  soem/ethercatfoe.h
  soem/ethercat.h
  soem/ethercatmain.h
  soem/ethercatprint.h
  soem/ethercatsoe.h
  soem/ethercattype.h
  osal/osal.h
  DESTINATION include/soem
  )

if(BUILD_TESTS) 
  add_subdirectory(test/linux/slaveinfo)
  add_subdirectory(test/linux/eepromtool)
  add_subdirectory(test/linux/simple_test)
endif()
