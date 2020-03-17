set(SOURCES my_test.c)
add_executable(my_test ${SOURCES})
target_link_libraries(my_test soem)
install(TARGETS my_test DESTINATION bin)
