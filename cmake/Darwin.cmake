# Licensed under the GNU General Public License version 2 with exceptions. See
# LICENSE file in the project root for full license information

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

target_sources(soem PRIVATE
  osal/macosx/osal.c
  osal/macosx/osal_defs.h
  oshw/macosx/oshw.c
  oshw/macosx/oshw.h
  oshw/macosx/nicdrv.c
  oshw/macosx/nicdrv.h
  )

target_include_directories(soem PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/osal/macosx>
  $<INSTALL_INTERFACE:include/soem>
  )

target_include_directories(soem PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/oshw/macosx>
  $<INSTALL_INTERFACE:include/soem>
  )

add_definitions(-Wall -Wextra -Werror)

target_link_libraries(soem PUBLIC pthread pcap)

install(FILES
  osal/macosx/osal_defs.h
  DESTINATION include/soem
  )
