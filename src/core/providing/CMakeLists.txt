cmake_minimum_required(VERSION 3.14)

find_package(Boost MODULE REQUIRED)

add_library(providing
    ProvidingHandler.h
    ProvidingHandler.cpp
    Provider.h
    Provider.cpp)

target_include_directories(providing
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(providing
    PRIVATE
        cxx_std_20
)

target_link_libraries(providing
    PUBLIC
        Boost::system
        crypto)
