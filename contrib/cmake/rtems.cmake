# This software is dual-licensed under GPLv3 and a commercial
# license. See the file LICENSE.md distributed with this software for
# full license information.

set(BUILD_TESTS FALSE)

target_sources(soem PRIVATE
  osal/rtems/osal.c
  osal/rtems/osal_defs.h
  oshw/rtems/oshw.c
  oshw/rtems/oshw.h
  oshw/rtems/nicdrv.c
  oshw/rtems/nicdrv.h
  )

target_include_directories(soem PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/osal/rtems>
  $<INSTALL_INTERFACE:include/soem>
  )

target_include_directories(soem PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/oshw/rtems>
  $<INSTALL_INTERFACE:include/soem>
  )

add_definitions(-Wall -Wextra -Werror)

target_link_libraries(soem PUBLIC rtemscpu bsd ${HOST_LIBS})

install(FILES
  osal/rtems/osal_defs.h
  oshw/rtems/nicdrv.h
  DESTINATION include/soem
  )
