# Licensed under the GNU General Public License version 2 with exceptions. See
# LICENSE file in the project root for full license information

target_sources(soem PRIVATE
  osal/rtk/osal.c
  osal/rtk/osal_defs.h
  oshw/rtk/oshw.c
  oshw/rtk/oshw.h
  oshw/rtk/nicdrv.c
  oshw/rtk/nicdrv.h
  oshw/rtk/lw_mac/lw_emac.c
  oshw/rtk/lw_mac/lw_emac.h
  oshw/rtk/fec/fec_ecat.c
  oshw/rtk/fec/fec_ecat.h
  )

target_include_directories(soem PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/osal/rtk>
  $<INSTALL_INTERFACE:include/soem>
  )

target_include_directories(soem PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/oshw/rtk>
  $<INSTALL_INTERFACE:include/soem>
  )

add_definitions(
  -D${BSP}
  -g
  -Wall
  -Wextra
  -Werror
  -Wno-unused-parameter
  -Wno-format
  )

install(FILES
  osal/rtk/osal_defs.h
  DESTINATION include/soem
  )
