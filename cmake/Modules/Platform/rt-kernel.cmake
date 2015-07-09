if(__RTK_CMAKE_INCLUDED)
  return()
endif()
set(__RTK_CMAKE_INCLUDED TRUE)
message("rt-kernel.cmake")

include_directories(
  ${RT_KERNEL_PATH}/include
  ${RT_KERNEL_PATH}/include/kern
  ${RT_KERNEL_PATH}/kern
  ${RT_KERNEL_PATH}/include/drivers
  ${RT_KERNEL_PATH}/include/arch/${ARCH}
  ${RT_KERNEL_PATH}/bsp/${BSP}/include
  ${RT_KERNEL_PATH}/lwip/src/include
  ${RT_KERNEL_PATH}/lwip/src/include/ipv4
)

link_directories(
  ${RT_KERNEL_PATH}/lib/${ARCH}
)
