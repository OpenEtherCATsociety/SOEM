add_executable(slaveinfo slaveinfo.c)
target_link_libraries(slaveinfo soem)
install(TARGETS slaveinfo DESTINATION bin)
