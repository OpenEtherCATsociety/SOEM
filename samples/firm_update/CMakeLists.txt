add_executable(firm_update firm_update.c)
target_link_libraries(firm_update soem)
install(TARGETS firm_update DESTINATION bin)
