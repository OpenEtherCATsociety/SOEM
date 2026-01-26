cmake_minimum_required(VERSION 3.16)

# Project name + language
project(moog_valve_app C)

# ---- Options ----
# Point this at the SOEM repo root (default assumes: SOEM/samples/moog_valve_app)
set(SOEM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "Path to SOEM repository root")

# ---- Executable ----
add_executable(moog_valve_app
  moog_valve_app.c
)

# ---- Include paths ----
# Your code includes:  #include "soem/soem.h"
# That typically means SOEM_ROOT is on the include path.
target_include_directories(moog_valve_app PRIVATE
  "${SOEM_ROOT}"
  "${SOEM_ROOT}/soem"
  "${SOEM_ROOT}/osal"
  "${SOEM_ROOT}/oshw"
)

# ---- Platform-specific SOEM sources ----
# SOEM is usually built by compiling core + OSAL + NIC driver.
# This setup matches typical Linux/Raspberry Pi builds.
file(GLOB SOEM_CORE_SRCS
  "${SOEM_ROOT}/soem/*.c"
)

file(GLOB SOEM_OSAL_LINUX_SRCS
  "${SOEM_ROOT}/osal/linux/*.c"
)

file(GLOB SOEM_OSHW_LINUX_SRCS
  "${SOEM_ROOT}/oshw/linux/*.c"
)

target_sources(moog_valve_app PRIVATE
  ${SOEM_CORE_SRCS}
  ${SOEM_OSAL_LINUX_SRCS}
  ${SOEM_OSHW_LINUX_SRCS}
)

# ---- Compile options ----
target_compile_features(moog_valve_app PRIVATE c_std_99)
target_compile_options(moog_valve_app PRIVATE
  -O2 -Wall -Wextra
)

# ---- Link libs ----
find_package(Threads REQUIRED)
target_link_libraries(moog_valve_app PRIVATE Threads::Threads)

# Some systems need librt for clock/time; harmless if not needed.
# (On newer glibc it's often unnecessary.)
target_link_libraries(moog_valve_app PRIVATE rt)

# ---- Output ----
set_target_properties(moog_valve_app PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
