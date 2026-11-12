cmake_minimum_required(VERSION 3.20)

option(ABFP_WITH_ANGBAND "Build the real pinned Angband adapter" OFF)

set(ABFP_BRIDGE_SOURCES
    src/abfp_bridge.c
    src/abfp_adapter.c
    src/abfp_frontend.c
)

if(ABFP_WITH_ANGBAND)
    list(APPEND ABFP_BRIDGE_SOURCES src/abfp_upstream_adapter.c)
endif()

add_library(abfp_bridge STATIC ${ABFP_BRIDGE_SOURCES})

target_include_directories(abfp_bridge PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include"
    "${CMAKE_CURRENT_LIST_DIR}/../../protocol"
)

target_compile_features(abfp_bridge PUBLIC c_std_99)

if(ABFP_WITH_ANGBAND)
    set(ANGBAND_SRC_DIR
        "${CMAKE_CURRENT_LIST_DIR}/../upstream/angband/src"
        CACHE PATH "Configured Angband source directory")
    target_include_directories(abfp_bridge PRIVATE "${ANGBAND_SRC_DIR}")
    target_compile_definitions(abfp_bridge PRIVATE ABFP_WITH_ANGBAND=1)
endif()
