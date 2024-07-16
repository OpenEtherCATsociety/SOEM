
set(SOURCES red_test.c)
add_executable(red_test ${SOURCES})
target_link_libraries(red_test soem)
install(TARGETS red_test DESTINATION bin)
