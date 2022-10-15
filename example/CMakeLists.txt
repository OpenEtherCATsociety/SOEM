cmake_minimum_required(VERSION 3.9)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake/Modules")
project(SOEMAPP
    DESCRIPTION "Simple Open EtherCAT Master Example"
    VERSION 1.4.0
    LANGUAGES C)

find_package(soem)

add_subdirectory(slaveinfo)

