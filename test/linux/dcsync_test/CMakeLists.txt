set(SOURCES dcsync_test.c)
add_executable(dcsync_test ${SOURCES})
target_link_libraries(dcsync_test soem)
install(TARGETS dcsync_test DESTINATION bin)
