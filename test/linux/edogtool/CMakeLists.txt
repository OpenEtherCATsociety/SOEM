
set(SOURCES edogtool.c)
add_executable(edogtool ${SOURCES})
target_link_libraries(edogtool soem)
install(TARGETS edogtool DESTINATION bin)
