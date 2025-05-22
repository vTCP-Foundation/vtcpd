cmake_minimum_required(VERSION 3.14)

add_library(interface__results
    interface/ResultsInterface.cpp
    interface/ResultsInterface.h
    result/CommandResult.cpp
    result/CommandResult.h)

target_include_directories(interface__results
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/core/interface
)

target_compile_features(interface__results
    PRIVATE
        cxx_std_20
)

target_link_libraries(interface__results
    PUBLIC
        core_libs
        common
        exceptions)
