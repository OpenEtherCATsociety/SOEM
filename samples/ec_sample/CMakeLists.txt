add_executable(ec_sample ec_sample.c)
target_link_libraries(ec_sample soem)
install(TARGETS ec_sample DESTINATION bin)
