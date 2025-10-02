cmake_minimum_required(VERSION 3.14)

add_subdirectory(lib)

add_library(paths
    PathsManager.h
    PathsManager.cpp
    ExchangePathsManager.h
    ExchangePathsManager.cpp)

target_include_directories(paths
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(paths
    PRIVATE
        cxx_std_20
)

target_link_libraries(paths
    PRIVATE
        slib_paths
)

# Add OR-Tools linking when available
if(ortools_FOUND)
   target_link_libraries(paths PUBLIC ortools::ortools)
endif()
