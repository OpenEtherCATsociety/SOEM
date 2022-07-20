
set(SOURCES eoe_test.c)
add_executable(eoe_test ${SOURCES})
target_link_libraries(eoe_test soem)
install(TARGETS eoe_test DESTINATION bin)
