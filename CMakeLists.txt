cmake_minimum_required(VERSION 3.20)
project(angband_first_person_bridge C)

include(CTest)
add_subdirectory(engine/bridge)

if(BUILD_TESTING)
    add_executable(abfp_bridge_tests tests/bridge_tests.c)
    target_link_libraries(abfp_bridge_tests PRIVATE abfp_bridge)
    add_test(NAME abfp_bridge_tests COMMAND abfp_bridge_tests)
endif()
