add_executable(eoe_test eoe_test.c)
target_link_libraries(eoe_test soem)
install(TARGETS eoe_test DESTINATION bin)
