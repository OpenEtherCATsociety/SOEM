add_executable(red_test red_test.c)
target_link_libraries(red_test soem)
install(TARGETS red_test DESTINATION bin)
