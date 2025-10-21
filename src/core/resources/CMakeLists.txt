cmake_minimum_required(VERSION 3.14)

add_library(resources_manager
    manager/ResourcesManager.h
    manager/ResourcesManager.cpp
    resources/BaseResource.h
    resources/BaseResource.cpp
    resources/PathsResource.h
    resources/PathsResource.cpp
    resources/ExchangePathsResource.h
    resources/ExchangePathsResource.cpp
    resources/BlockNumberRecourse.h
    resources/BlockNumberRecourse.cpp)

target_include_directories(resources_manager
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(resources_manager
    PRIVATE
        cxx_std_20
)

target_link_libraries(resources_manager)