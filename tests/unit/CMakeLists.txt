cmake_minimum_required(VERSION 3.14)

# Build only stable, up-to-date tests (other old tests require refactor)
add_executable(unit_tests
    database_config_test.cpp
    crypto/SphincsKeysTest.cpp
    crypto/SphincsSignatureTest.cpp
    sqlite/HistoryStorageSQLiteTest.cpp
    sqlite/SQLiteStatementRAIITest.cpp
    sqlite/AddressHandlerSQLiteTest.cpp
    sqlite/AuditHandlerSQLiteTest.cpp
    sqlite/AuditRulesHandlerSQLiteTest.cpp
    sqlite/ContractorKeysHandlerSQLiteTest.cpp
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

# Discover tests
gtest_discover_tests(unit_tests) 