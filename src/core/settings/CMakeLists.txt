cmake_minimum_required(VERSION 3.14)

add_library(settings
    DatabaseProviderType.h
    Settings.h
    Settings.cpp)

target_include_directories(settings
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(settings
    PRIVATE
        cxx_std_20
)

target_link_libraries(settings
    PUBLIC
        common)