add_executable(coetest coetest.c)
target_link_libraries(coetest soem)
install(TARGETS coetest DESTINATION bin)
