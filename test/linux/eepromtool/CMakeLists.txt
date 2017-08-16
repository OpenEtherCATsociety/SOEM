
set(SOURCES eepromtool.c)
add_executable(eepromtool ${SOURCES})
target_link_libraries(eepromtool soem)
install(TARGETS eepromtool DESTINATION bin)
