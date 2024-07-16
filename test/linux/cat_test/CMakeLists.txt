
set(SOURCES cat_test.c)
add_executable(cat_test ${SOURCES})
target_link_libraries(cat_test soem)
install(TARGETS cat_test DESTINATION bin)
