cmake_minimum_required(VERSION 3.14)

find_package(Boost REQUIRED COMPONENTS system)
add_subdirectory(messages)

add_library(observing
    ObservingHandler.h
    ObservingHandler.cpp
    ObservingTransaction.h
    ObservingTransaction.cpp
    ObservingCommunicator.h
    ObservingCommunicator.cpp)

target_include_directories(observing
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(observing
    PRIVATE
        cxx_std_20
)

target_link_libraries(observing
    PUBLIC
        observing__messages
        Boost::system)