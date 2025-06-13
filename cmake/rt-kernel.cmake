# Licensed under the GNU General Public License version 2 with exceptions. See
# LICENSE file in the project root for full license information

find_package(rtkernel)
find_package(${BSP})

target_sources(soem PRIVATE
  osal/rtk/osal.c
  osal/rtk/osal_defs.h
  oshw/rtk/oshw.c
  oshw/rtk/oshw.h
  oshw/rtk/nicdrv.c
  oshw/rtk/nicdrv.h
  oshw/rtk/lw_mac/lw_emac.c
  oshw/rtk/fec/fec_ecat.c
  oshw/rtk/fec/fec_ecat.h
)

target_include_directories(soem PUBLIC
  $<BUILD_INTERFACE:${SOEM_SOURCE_DIR}/osal/rtk>
  $<INSTALL_INTERFACE:include/soem>
)

target_include_directories(soem PUBLIC
  $<BUILD_INTERFACE:${SOEM_SOURCE_DIR}/oshw/rtk>
  $<INSTALL_INTERFACE:include/soem>
)

foreach(target IN ITEMS
    soem
    coetest
    eepromtool
    eni_test
    eoe_test
    firm_update
    red_test
    simple_ng
    simple_test
    slaveinfo)
  if (TARGET ${target})
    target_compile_options(${target} PRIVATE
      -Wall
      -Wextra
      -Wno-unused-parameter
      -Wno-format
    )
  endif()
endforeach()

install(FILES
  osal/rtk/osal_defs.h
  oshw/rtk/nicdrv.h
  DESTINATION include/soem
)

target_link_libraries(soem PUBLIC kern ${BSP})
