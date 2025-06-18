cmake_minimum_required(VERSION 3.14)

add_library(equivalents
    EquivalentsSubsystemsRouter.h
    EquivalentsSubsystemsRouter.cpp
    EquivalentsCyclesSubsystemsRouter.h
    EquivalentsCyclesSubsystemsRouter.cpp)

target_include_directories(equivalents
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(equivalents
    PRIVATE
        cxx_std_20
)

target_link_libraries(equivalents
    PUBLIC
        core_libs
        trust_lines 
        topology
        cycles
        paths
        delayed_tasks)