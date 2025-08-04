cmake_minimum_required(VERSION 3.14)

# Include GoogleTest
include(GoogleTest)

# Add unit tests subdirectory
add_subdirectory(unit)

# Add storage integration tests
add_subdirectory(storage) 