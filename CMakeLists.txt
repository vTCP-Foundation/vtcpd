cmake_minimum_required(VERSION 3.21)

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

# Use modern behavior for find_package ROOT variables to silence dev warnings
if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

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
target_link_libraries(project_options INTERFACE Threads::Threads)

# Ensure local third_party headers (e.g., absl overrides) are found first
target_include_directories(project_options INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Make Boost headers available project-wide
if(TARGET Boost::headers)
    target_link_libraries(project_options INTERFACE Boost::headers)
endif()

# Provide a fallback header-only Boost.System target for Boost >= 1.89
if(NOT TARGET Boost::system)
    add_library(Boost::system INTERFACE IMPORTED)
    target_compile_definitions(Boost::system INTERFACE BOOST_ERROR_CODE_HEADER_ONLY=1)
endif()

# Dependencies management
include(FetchContent)

# Google Test Framework
option(BUILD_TESTING "Build the testing tree" OFF)
if(BUILD_TESTING)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(googletest)
    
    enable_testing()
    add_subdirectory(tests)
endif()

# Find required packages
# Prefer Boost CMake config (CONFIG) when available (e.g., Boost 1.87). Fallback to FindBoost (MODULE).
set(_boost_components
    filesystem
    thread
    date_time
)
find_package(Boost CONFIG QUIET COMPONENTS ${_boost_components})
if(NOT Boost_FOUND)
    set(Boost_NO_BOOST_CMAKE ON)
    find_package(Boost MODULE REQUIRED COMPONENTS ${_boost_components})
endif()
find_package(Threads REQUIRED)
find_package(ortools REQUIRED)
message(STATUS "Boost version: ${Boost_VERSION_STRING}")
message(STATUS "OR-Tools is required and will be linked")
# Database backend selection
# Usage: cmake -S . -B build -DDB_TYPE=sqlite|postgresql
set(DB_TYPE "sqlite" CACHE STRING "Database backend: sqlite|postgresql")
set_property(CACHE DB_TYPE PROPERTY STRINGS "sqlite" "postgresql")

if(DB_TYPE STREQUAL "sqlite")
    find_package(SQLite3 REQUIRED)
    set(DB_LIB_TARGET SQLite::SQLite3)
    message(STATUS "Database backend: SQLite3")
    target_compile_definitions(project_options INTERFACE DB_SQLITE)
elseif(DB_TYPE STREQUAL "postgresql")
    find_package(PostgreSQL REQUIRED)
    set(DB_LIB_TARGET PostgreSQL::PostgreSQL)
    message(STATUS "Database backend: PostgreSQL")
    target_compile_definitions(project_options INTERFACE DB_POSTGRESQL)
else()
    message(FATAL_ERROR "Unknown DB_TYPE='${DB_TYPE}'. Use 'sqlite' or 'postgresql'.")
endif()



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
    TESTS
)

target_compile_definitions(project_options INTERFACE
    $<$<CONFIG:Debug>:${DEBUG_DEFINITIONS}>
)

# Compiler options
target_compile_options(project_options INTERFACE
    $<$<CONFIG:Release>:-O2 -Os -s -fdata-sections -fmerge-all-constants>
    $<$<CONFIG:Debug>:-g -O0>
    # Add GCC specific sanitizer compile flags for Debug builds if ENABLE_GCC_SANITIZERS is ON
    $<$<AND:$<CONFIG:Debug>,$<BOOL:${ENABLE_GCC_SANITIZERS}>,$<CXX_COMPILER_ID:GNU>>:
        -fno-omit-frame-pointer
        -fsanitize=address,undefined
    >
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

# Testing configuration removed - legacy Catch2 tests deleted

option(ENABLE_GCC_SANITIZERS "Enable GCC runtime memory sanitizers (ASan, UBSan) for Debug builds" OFF)

# Core libraries
add_library(core_libs INTERFACE)
target_link_libraries(core_libs INTERFACE
    project_options
    Threads::Threads
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
    core/rates
)
    add_subdirectory(src/${COMPONENT})
endforeach()

# External libraries

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
    rates

    # Libs
    sodium
)

# Link chosen DB backend (shared/dynamic linking via imported target)
target_link_libraries(vtcpd PRIVATE ${DB_LIB_TARGET})

# Link OR-Tools (hard dependency)
target_link_libraries(vtcpd PRIVATE ortools::ortools)

# Add GCC specific sanitizer link flags for Debug builds for the main executable if ENABLE_GCC_SANITIZERS is ON
target_link_options(vtcpd PRIVATE
    $<$<AND:$<CONFIG:Debug>,$<BOOL:${ENABLE_GCC_SANITIZERS}>,$<CXX_COMPILER_ID:GNU>>:
        -fno-omit-frame-pointer
        -fsanitize=address,undefined
    >
)

# Installation
include(GNUInstallDirs)
install(TARGETS vtcpd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
