cmake_minimum_required(VERSION 3.20)
add_library(abfp_bridge STATIC
    src/abfp_bridge.c
    src/abfp_adapter.c
)
target_include_directories(abfp_bridge PUBLIC include ../../protocol)
target_compile_features(abfp_bridge PUBLIC c_std_99)
