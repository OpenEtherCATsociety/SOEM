add_executable(eepromtool eepromtool.c)
target_link_libraries(eepromtool soem)
install(TARGETS eepromtool DESTINATION bin)
