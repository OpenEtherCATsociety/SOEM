include(AddENI)

add_executable(eni_test eni_test.c)
add_eni(eni_test
  sample-eni.xml
)
target_link_libraries(eni_test soem)
install(TARGETS eni_test DESTINATION bin)
