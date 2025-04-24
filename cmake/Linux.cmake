# Licensed under the GNU General Public License version 2 with exceptions. See
# LICENSE file in the project root for full license information

target_sources(soem PRIVATE
  osal/linux/osal.c
  osal/linux/osal_defs.h
  oshw/linux/oshw.c
  oshw/linux/oshw.h
  oshw/linux/nicdrv.c
  oshw/linux/nicdrv.h
)

target_include_directories(soem PUBLIC
  $<BUILD_INTERFACE:${SOEM_SOURCE_DIR}/osal/linux>
  $<BUILD_INTERFACE:${SOEM_SOURCE_DIR}/oshw/linux>
  $<INSTALL_INTERFACE:include/soem>
)

target_compile_options(soem PRIVATE
  -Wall
  -Wextra
  -Werror
)

target_link_libraries(soem PUBLIC pthread rt)

install(FILES
  osal/linux/osal_defs.h
  oshw/linux/nicdrv.h
  DESTINATION include/soem
)
