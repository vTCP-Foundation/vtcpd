cmake_minimum_required(VERSION 3.14)

set(SOURCE_FILES

        CyclesManager.h
        CyclesManager.cpp

        RoutingTableManager.h
        RoutingTableManager.cpp
        
        CyclesRunningParameters.h
        CyclesRunningParameters.cpp)

target_include_directories(cycles
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(cycles
    PRIVATE
        cxx_std_20
)