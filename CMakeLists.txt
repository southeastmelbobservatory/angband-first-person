cmake_minimum_required(VERSION 3.20)
project(angband_first_person_bridge C)

include(CTest)
add_subdirectory(engine/bridge)

if(BUILD_TESTING)
    add_executable(abfp_bridge_tests tests/bridge_tests.c)
    target_link_libraries(abfp_bridge_tests PRIVATE abfp_bridge)
    add_test(NAME abfp_bridge_tests COMMAND abfp_bridge_tests)

    find_package(Threads REQUIRED)
    add_executable(abfp_transport_tests
        tests/transport_tests.c
        engine/host/src/abfp_transport.c
    )
    target_include_directories(abfp_transport_tests PRIVATE
        engine/host/include
        protocol
    )
    target_link_libraries(abfp_transport_tests PRIVATE Threads::Threads)
    add_test(NAME abfp_transport_tests COMMAND abfp_transport_tests)
endif()
