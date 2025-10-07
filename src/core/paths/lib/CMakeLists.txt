cmake_minimum_required(VERSION 3.14)

add_library(slib_paths
    Path.h
    Path.cpp
    PathsCollection.h
    PathsCollection.cpp
    ExchangePath.h
    ExchangePath.cpp
    OptimalPathResult.h
    OptimalPathResult.cpp)

target_include_directories(slib_paths
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(slib_paths
    PRIVATE
        cxx_std_20
)

target_link_libraries(slib_paths)
