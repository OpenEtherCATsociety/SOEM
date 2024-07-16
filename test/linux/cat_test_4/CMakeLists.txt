# enable_language(C)
# enable_language(CXX)

# set(CMAKE_C_COMPILER gcc)
# set(CMAKE_CXX_COMPILER g++)
set(SOURCES cat_test_4.c)
add_executable(cat_test_4 ${SOURCES})
target_link_libraries(cat_test_4 soem)
install(TARGETS cat_test_4 DESTINATION bin)
