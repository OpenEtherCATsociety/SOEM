
add_executable(eepromtool main.c)
target_link_libraries(eepromtool soem)
install(TARGETS eepromtool DESTINATION bin)
