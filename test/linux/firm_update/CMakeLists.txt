
set(SOURCES firm_update.c)
add_executable(firm_update ${SOURCES})
target_link_libraries(firm_update soem)
install(TARGETS firm_update DESTINATION bin)
