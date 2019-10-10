
add_executable(slaveinfo main.c)
target_link_libraries(slaveinfo soem)
install(TARGETS slaveinfo DESTINATION bin)
