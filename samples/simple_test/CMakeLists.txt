add_executable(simple_test simple_test.c)
target_link_libraries(simple_test soem)
install(TARGETS simple_test DESTINATION bin)
