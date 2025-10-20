cmake_minimum_required(VERSION 3.14)

add_library(transactions__find_path
    FindPathByMaxFlowTransaction.h
    FindPathByMaxFlowTransaction.cpp
    FindPathsByMaxFlowExchangeTransaction.h
    FindPathsByMaxFlowExchangeTransaction.cpp)

target_include_directories(transactions__find_path
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(transactions__find_path
    PRIVATE
        cxx_std_20
)

target_link_libraries(transactions__find_path
    PUBLIC
        transactions__base
        resources_manager
        io__storage)