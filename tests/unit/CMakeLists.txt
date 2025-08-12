cmake_minimum_required(VERSION 3.14)

# Build only stable, up-to-date tests (other old tests require refactor)
add_executable(unit_tests
    database_config_test.cpp
    crypto/SphincsKeysTest.cpp
    crypto/SphincsSignatureTest.cpp
    crypto/SphincsMemoryLeakTest.cpp
    sqlite/HistoryStorageSQLiteTest.cpp
    sqlite/SQLiteStatementRAIITest.cpp
    sqlite/AddressHandlerSQLiteTest.cpp
    sqlite/AuditHandlerSQLiteTest.cpp
    sqlite/AuditRulesHandlerSQLiteTest.cpp
    sqlite/ContractorsHandlerSQLiteTest.cpp
    sqlite/FeaturesHandlerSQLiteTest.cpp
    sqlite/IncomingPaymentReceiptHandlerSQLiteTest.cpp
    sqlite/IOTransactionSQLiteTest.cpp
    sqlite/OutgoingPaymentReceiptHandlerSQLiteTest.cpp
)

# Include directories for tests
target_include_directories(unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)

# Link libraries
target_link_libraries(unit_tests PRIVATE
    # Google Test and Mock
    gtest_main
    gmock_main
    
    # Project executable/library for core definitions
    io__storage
    contractors
    common
    logger
    core_libs
    exceptions
    crypto
    trust_lines
    settings
)

# Set C++ standard
target_compile_features(unit_tests PRIVATE cxx_std_20)

# Optional Address/Leak/Undefined sanitizers for unit tests only
option(ENABLE_TEST_SANITIZERS "Enable ASan/LSan/UBSan for unit tests" OFF)
if(ENABLE_TEST_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(TEST_SAN_FLAGS -fsanitize=address,leak,undefined -fno-omit-frame-pointer -g)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # LeakSanitizer is part of ASan on GCC; enable leak detection via ASAN_OPTIONS at runtime
        set(TEST_SAN_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    else()
        set(TEST_SAN_FLAGS "")
    endif()
    if(TEST_SAN_FLAGS)
        target_compile_options(unit_tests PRIVATE ${TEST_SAN_FLAGS})
        target_link_options(unit_tests PRIVATE ${TEST_SAN_FLAGS})
    endif()
endif()

# Discover tests
gtest_discover_tests(unit_tests)