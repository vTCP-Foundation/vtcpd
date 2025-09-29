cmake_minimum_required(VERSION 3.14)

# Use modern Boost configuration
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost MODULE REQUIRED)

add_subdirectory(messages)
add_subdirectory(communicator)
