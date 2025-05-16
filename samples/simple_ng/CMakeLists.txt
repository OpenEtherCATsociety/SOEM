set(SOURCES simple_ng.c)
add_executable(simple_ng ${SOURCES})
target_link_libraries(simple_ng soem)
install(TARGETS simple_ng DESTINATION bin)
