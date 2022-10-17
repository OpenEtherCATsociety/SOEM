cmake_minimum_required(VERSION 3.9)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake/Modules")
project(SOEMAPP
    DESCRIPTION "Simple Open EtherCAT Master Example App"
    VERSION 1.4.0
    LANGUAGES C)

#################################################
## Generally used variables for third parties
SET(3RDPARTY_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/third_parties)
include(cmake/ThirdPartyHandling.cmake)
set(THIRD_PARTY_CONFIG Debug;Release)
set(GIT_NAMES GIT Git)

########## SOEM ###################
set(SOEM_COMMAND "git clone https://github.com/kutij/SOEM.git")
set(SOEM_CACHE_SETTINGS )

third_party_policy(soem ${SOEM_COMMAND} SOEM_CACHE_SETTINGS THIRD_PARTY_CONFIG true GIT_NAMES)
macro(requires_soem NAME)
	target_link_libraries (${NAME} soem)
endmacro(requires_soem)

add_subdirectory(slaveinfo)

