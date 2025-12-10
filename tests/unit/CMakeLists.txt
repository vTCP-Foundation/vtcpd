cmake_minimum_required(VERSION 3.14)

# Build only stable, up-to-date tests (other old tests require refactor)
add_executable(unit_tests
    database_config_test.cpp
    common/ExchangeRateMathTest.cpp
    crypto/SphincsKeysTest.cpp
    crypto/SphincsSignatureTest.cpp
    crypto/SphincsMemoryLeakTest.cpp
    sqlite/AddressHandlerSQLiteTest.cpp
    sqlite/AuditHandlerSQLiteTest.cpp
    sqlite/AuditRulesHandlerSQLiteTest.cpp
    sqlite/ContractorKeysHandlerSQLiteTest.cpp
    sqlite/ContractorsHandlerSQLiteTest.cpp
    sqlite/FeaturesHandlerSQLiteTest.cpp
    sqlite/HistoryStorageSQLiteTest.cpp
    sqlite/IncomingPaymentReceiptHandlerSQLiteTest.cpp
    sqlite/IOTransactionSQLiteTest.cpp
    sqlite/OutgoingPaymentReceiptHandlerSQLiteTest.cpp
    sqlite/OwnKeysHandlerSQLiteTest.cpp
    sqlite/PaymentKeysHandlerSQLiteTest.cpp
    sqlite/PaymentParticipantsVotesHandlerSQLiteTest.cpp
    sqlite/PaymentTransactionsHandlerSQLiteTest.cpp
    sqlite/SQLiteStatementRAIITest.cpp
    sqlite/StorageHandlerSQLiteTest.cpp
    sqlite/TransactionsHandlerSQLiteTest.cpp
    sqlite/TrustLineHandlerSQLiteTest.cpp
    postgresql/PaymentKeysHandlerPostgreSQLUnitTest.cpp
    rates/ExchangeRateTest.cpp
    rates/ExchangeRatesManagerTest.cpp
    equivalents/EquivalentsSubsystemsRouterTest.cpp
    equivalents/EquivalentsSubsystemsRouterParticipantsTest.cpp
    network/TailManagerTest.cpp
    network/IncomingRemoteNodeTest.cpp
    network/messages/IntermediateNodeReservationRequestMessageTest.cpp
    network/messages/IntermediateNodeReservationResponseMessageTest.cpp
    network/messages/TestRequestMessageWithReservations.cpp
    network/messages/TestFinalAmountsConfigurationMessage.cpp
    network/messages/TestTransactionPublicKeyHashMessage.cpp
    network/messages/CoordinatorReservationConditionsMessageTest.cpp
    commissions/CommissionsManagerTest.cpp
    commissions/TopologyTrustLinesManagerCommissionsTest.cpp
    topology/TopologyTrustLinesManagerMutationTest.cpp
    commands/InitiateMaxFlowExchangeCalculationCommandTest.cpp
    transactions/InitiateMaxFlowExchangeCalculationTransactionORToolsTest.cpp
    transactions/InitiateMaxFlowExchangeCalculationApplyLogicTest.cpp
    transactions/InitiateMaxFlowExchangeCalculationLargeTopologyTest.cpp
    transactions/InitiateMaxFlowExchangeCalculationHugeTopologyTest.cpp
    transactions/InitiateMaxFlowExchangeCalculationMegaTopologyTest.cpp
    transactions/InitiateMaxFlowExchangeCalculationCommissionTest.cpp
    transactions/InitiateMaxFlowExchangeCalculationExchangeCommissionTest.cpp
    transactions/InitiateMaxFlowExchangeCalculationUniqueCommissionTest.cpp
    transactions/InitiateMaxFlowExchangeCalculationMixedCommissionTest.cpp
    transactions/FindPathsByMaxFlowExchangeTransactionTest.cpp
    paths/ExchangePathsManagerTest.cpp
    paths/PathSimulationTest.cpp
    paths/TestOptimalPathResult.cpp
    paths/TestExchangePath.cpp
    resources/ExchangePathsResourceTest.cpp
    resources/ResourcesManagerSignalTest.cpp
    transactions/TestPathReservation.cpp
    transactions/EstimationTransactionsTest.cpp
    transactions/TestBaseExchangePaymentTransaction.cpp
    transactions/TestCoordinatorExchangePaymentTransaction.cpp
    transactions/TestCoordinatorPathProcessing.cpp
    transactions/TestReceiverExchangePaymentTransaction.cpp
    transactions/TestIntermediateNodeCheckReservations.cpp
    transactions/TestBasePaymentTransactionSerialization.cpp
    transactions/TestCoordinatorExchangePaymentCapacityAdjustment.cpp
    transactions/CoordinatorExchangePaymentPathRebuildingTest.cpp
    transactions/TestIntermediateNodeExchangePaymentIncoming.cpp
    transactions/TestIntermediateNodeConditionValidation.cpp
    transactions/TestCoordinatorConditionChangeHandling.cpp
    transactions/TestExchangePaymentConditionsRegression.cpp
    transactions/CoordinatorExchangePaymentAllowableAmountTest.cpp
    payments/TestAmountReservation.cpp
    payments/TestAmountReservationsHandler.cpp
    core/CoreORToolsStartupTest.cpp
    core/CoreSignalConnectionTest.cpp
    observing/ObservingPaymentClaimTest.cpp
)

# Include directories for tests
target_include_directories(unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)

# Force OpenSSL 3.5+ include/lib paths for unit tests build (requested)
# These complement the configure-time flags and ensure headers/libs are preferred
target_compile_options(unit_tests PRIVATE -isystem /usr/local/openssl/include)
target_link_directories(unit_tests PRIVATE /usr/local/openssl/lib)

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
    # Ensure OpenSSL symbols are resolved against the specified installation
    ssl
    crypto
    trust_lines
    rates
    equivalents
    topology
    paths
    interface__events
    interface__commands
    interface__results
    settings
    messages__base__transaction
    messages__base__max_flow_calculation
    messages__max_flow_calculation
    transactions__base
    network__messages
    network__communicator
    observing
    transactions
    transactions__payments
    observing__messages
    subsystems_controller
    resources_manager
)

# Set C++ standard
target_compile_features(unit_tests PRIVATE cxx_std_20)

# Discover tests
gtest_discover_tests(unit_tests) 

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

# Register unit tests as a single CTest entry to avoid NOT_BUILT placeholder
add_test(NAME unit_tests COMMAND $<TARGET_FILE:unit_tests>)
