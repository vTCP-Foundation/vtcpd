cmake_minimum_required(VERSION 3.14)

find_package(Boost MODULE REQUIRED)

find_package(nlohmann_json CONFIG QUIET)
if(NOT TARGET nlohmann_json::nlohmann_json)
    # Fallback to bundled headers when system package is unavailable.
    add_library(nlohmann_json INTERFACE)
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
    target_include_directories(nlohmann_json
        INTERFACE
            ${CMAKE_SOURCE_DIR}/src/libs/json)
endif()

if(NOT TARGET Boost::asio)
    # Ensure a header-only Boost.Asio target exists for consistent linking.
    add_library(Boost::asio INTERFACE IMPORTED)
    target_include_directories(Boost::asio
        INTERFACE
            ${Boost_INCLUDE_DIRS})
endif()

add_library(network__rpc
    RpcMethod.h
    RpcResponseStatus.h
    RpcRequest.cpp
    RpcRequest.h
    RpcResponse.cpp
    RpcResponse.h)

target_include_directories(network__rpc
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/requests
        ${CMAKE_CURRENT_SOURCE_DIR}/responses
)

target_compile_features(network__rpc
    PRIVATE
        cxx_std_20
)

target_link_libraries(network__rpc
    PUBLIC
        common
        logger
        contractors
        observing
        Boost::asio
        nlohmann_json::nlohmann_json)
