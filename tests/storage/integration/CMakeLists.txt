cmake_minimum_required(VERSION 3.14)

# Check for PostgreSQL availability
find_package(PostgreSQL REQUIRED)

# Build PostgreSQL integration tests
add_executable(postgresql_integration_tests
    # Helper classes
    fixtures/DatabaseTestHelper.cpp
    fixtures/PostgreSQLTestFixtures.cpp
    
    # Integration tests for individual handlers
    postgresql/AddressHandlerPostgreSQLIntegrationTest.cpp
    postgresql/ContractorsHandlerPostgreSQLIntegrationTest.cpp
    postgresql/TrustLineHandlerPostgreSQLIntegrationTest.cpp
    postgresql/OwnKeysHandlerPostgreSQLIntegrationTest.cpp
    postgresql/ContractorKeysHandlerPostgreSQLIntegrationTest.cpp
    postgresql/AuditHandlerPostgreSQLIntegrationTest.cpp
    postgresql/AuditRulesHandlerPostgreSQLIntegrationTest.cpp
    postgresql/IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest.cpp
    postgresql/OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest.cpp
    postgresql/PaymentKeysHandlerPostgreSQLIntegrationTest.cpp
    postgresql/PaymentParticipantsVotesHandlerPostgreSQLIntegrationTest.cpp
    postgresql/PaymentTransactionsHandlerPostgreSQLIntegrationTest.cpp
    postgresql/TransactionsHandlerPostgreSQLIntegrationTest.cpp
    postgresql/FeaturesHandlerPostgreSQLIntegrationTest.cpp
    postgresql/CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest.cpp
)

# Include directories for tests
target_include_directories(postgresql_integration_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${PostgreSQL_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(postgresql_integration_tests PRIVATE
    # Google Test and Mock
    gtest_main
    gmock_main
    
    # PostgreSQL
    ${PostgreSQL_LIBRARIES}
    
    # Project libraries
    io__storage
    common
    contractors
    crypto
    logger
    settings
    trust_lines
    reservations
    exceptions
    transactions__base
)

# Set C++ standard
target_compile_features(postgresql_integration_tests PRIVATE cxx_std_20)

# Add GCC specific sanitizer compile flags for Debug builds if ENABLE_GCC_SANITIZERS is ON
target_compile_options(postgresql_integration_tests PRIVATE
    $<$<AND:$<CONFIG:Debug>,$<BOOL:${ENABLE_GCC_SANITIZERS}>,$<CXX_COMPILER_ID:GNU>>:
        -fno-omit-frame-pointer
        -fsanitize=address,undefined
    >
)

# Add GCC specific sanitizer link flags for Debug builds if ENABLE_GCC_SANITIZERS is ON
target_link_options(postgresql_integration_tests PRIVATE
    $<$<AND:$<CONFIG:Debug>,$<BOOL:${ENABLE_GCC_SANITIZERS}>,$<CXX_COMPILER_ID:GNU>>:
        -fno-omit-frame-pointer
        -fsanitize=address,undefined
    >
)

# Discover tests
gtest_discover_tests(postgresql_integration_tests) 