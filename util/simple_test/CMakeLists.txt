
add_executable(simple_test main.c)
target_link_libraries(simple_test soem)
install(TARGETS simple_test DESTINATION bin)
