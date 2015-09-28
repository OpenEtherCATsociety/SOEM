
message("rt-kernel-C.cmake")

# Determine toolchain
include(CMakeForceCompiler)

if(${ARCH} MATCHES "kinetis")
  cmake_force_c_compiler(arm-eabi-gcc GNU)
  cmake_force_cxx_compiler(arm-eabi-g++ GNU)
elseif(${ARCH} MATCHES "bfin")
  cmake_force_c_compiler(bfin-elf-gcc GNU)
  cmake_force_cxx_compiler(bfin-elf-g++ GNU)
endif()  
