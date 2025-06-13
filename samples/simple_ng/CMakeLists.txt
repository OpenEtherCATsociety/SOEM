add_executable(simple_ng simple_ng.c)
target_link_libraries(simple_ng soem)
install(TARGETS simple_ng DESTINATION bin)
