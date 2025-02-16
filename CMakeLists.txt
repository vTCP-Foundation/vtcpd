﻿cmake_minimum_required(VERSION 3.21)

project(vtcpd 
    VERSION 0.1.0
    DESCRIPTION "VTCP Network Daemon"
    LANGUAGES CXX)

# Global properties
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Set custom build types
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel" CACHE STRING "" FORCE)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Define compile features upfront
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_20)
target_compile_definitions(project_options INTERFACE 
    $<$<PLATFORM_ID:Linux>:LINUX>
    $<$<OR:$<CONFIG:Debug>>:DEBUG>
    TRANSACTIONS_LOG
    BOOST_BIND_GLOBAL_PLACEHOLDERS
)

# Dependencies management
include(FetchContent)

# Find required packages
find_package(Boost CONFIG REQUIRED
    COMPONENTS 
        system 
        filesystem 
        thread 
        date_time
)

# Debug compile definitions
set(DEBUG_DEFINITIONS
    INTERNAL_ARGUMENTS_VALIDATION
    DEBUG_LOG_NETWORK_COMMUNICATOR
    DEBUG_LOG_STORAGE_HANDLER
    DEBUG_LOG_TRANSACTIONS_MANAGER
    DEBUG_LOG_SCHEDULER
    DEBUG_LOG_TRUST_LINES_PROCESSING
    DEBUG_LOG_ROUTING_TABLES_PROCESSING
    DEBUG_LOG_MAX_FLOW_CALCULATION
    DEBUG_LOG_PATHS_COLLECTING
    DEBUG_LOG_PAYMENTS_PROCESSING
    DEBUG_LOG_CYCLES_PROCESSING
    DEBUG_LOG_CYCLES_BUILDING_POCESSING
    DEBUG_LOG_OBSEVING_HANDLER
    DEBUG_LOG_PROVIDING_HANDLER
)

target_compile_definitions(project_options INTERFACE
    $<$<CONFIG:Debug>:${DEBUG_DEFINITIONS}>
)

# Compiler options
target_compile_options(project_options INTERFACE
    $<$<CONFIG:Release>:-O2 -Os -s -fdata-sections -fmerge-all-constants>
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
)

# Engine configuration
option(ENGINE_TYPE_DC "Use DC engine type" ON)
option(ENGINE_TYPE_DECENTRALIZED "Use DECENTRALIZED engine type" OFF)
if(ENGINE_TYPE_DC)
    target_compile_definitions(project_options INTERFACE ENGINE_TYPE_DC)
elseif(ENGINE_TYPE_DECENTRALIZED)
    target_compile_definitions(project_options INTERFACE ENGINE_TYPE_DECENTRALIZED)
endif()

# Build type configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")

# Testing configuration
option(BUILD_TESTING "Build the testing tree." OFF)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(src/tests)
endif()

# Core libraries
add_library(core_libs INTERFACE)
target_link_libraries(core_libs INTERFACE
    project_options
    Boost::system
    Boost::filesystem
    Boost::thread
    Boost::date_time
)

# Core components
foreach(COMPONENT
    core/common
    core/common/exceptions
    core/logger
    core/interface
    core/network
    core/settings
    core/paths
    core/transactions
    core/trust_lines
    core/topology
    core/delayed_tasks
    core/payments/reservations
    core/io/storage
    core/resources
    core/cycles
    core/subsystems_controller
    core/equivalents
    core/crypto
    core/contractors
    core/observing
    core/features
    core/providing
)
    add_subdirectory(src/${COMPONENT})
endforeach()

# External libraries
add_subdirectory(src/libs/sqlite3)

# Main executable
add_executable(vtcpd
    main.cpp
    src/core/Core.cpp
    src/core/Core.h
    src/libs/json/json.h
)

target_include_directories(vtcpd PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_link_libraries(vtcpd PRIVATE
    core_libs
    logger
    settings
    network__communicator
    network__messages
    interface__commands
    interface__results
    interface__events
    paths
    trust_lines
    topology
    delayed_tasks
    transactions
    resources_manager
    cycles
    subsystems_controller
    equivalents
    crypto
    contractors
    observing
    features
    providing

    # Libs
    libsqlite3
    sodium
)

# Installation
include(GNUInstallDirs)
install(TARGETS vtcpd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
