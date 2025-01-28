
set(SOURCES slaveinfo.c)
add_executable(slaveinfo ${SOURCES})
target_link_libraries(slaveinfo soem)
install(TARGETS slaveinfo DESTINATION bin)
