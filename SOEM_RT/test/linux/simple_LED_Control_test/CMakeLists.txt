
set(SOURCES simple_LED_test.c)
add_executable(DRCL_LED_test ${SOURCES})
target_link_libraries(DRCL_LED_test soem)
install(TARGETS DRCL_LED_test DESTINATION bin)
