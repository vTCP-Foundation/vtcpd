cmake_minimum_required(VERSION 3.14)

# Check for PostgreSQL availability before building
find_package(PostgreSQL QUIET)

add_library(io__storage
    record/base/Record.h
    record/base/Record.cpp
    record/trust_line/TrustLineRecord.h
    record/trust_line/TrustLineRecord.cpp
    record/payment/PaymentRecord.h
    record/payment/PaymentRecord.cpp
    record/payment/PaymentAdditionalRecord.h
    record/payment/PaymentAdditionalRecord.cpp
    record/audit/AuditRecord.h
    record/audit/AuditRecord.cpp
    record/audit/ReceiptRecord.h
    record/audit/ReceiptRecord.cpp
    # Factory files
    StorageProviderFactory.h
    StorageProviderFactory.cpp
    # Interface files
    interfaces/StorageHandler.h
    interfaces/IOTransaction.h
    interfaces/CommunicatorStorageHandler.h
    interfaces/CommunicatorIOTransaction.h
    interfaces/TrustLineHandler.h
    interfaces/TransactionsHandler.h
    interfaces/HistoryStorage.h
    interfaces/CommunicatorMessagesQueueHandler.h
    interfaces/OwnKeysHandler.h
    interfaces/ContractorKeysHandler.h
    interfaces/AuditHandler.h
    interfaces/PaymentKeysHandler.h
    interfaces/IncomingPaymentReceiptHandler.h
    interfaces/OutgoingPaymentReceiptHandler.h
    interfaces/PaymentTransactionsHandler.h
    interfaces/PaymentParticipantsVotesHandler.h
    interfaces/AuditRulesHandler.h
    interfaces/ContractorsHandler.h
    interfaces/AddressHandler.h
    interfaces/FeaturesHandler.h
    interfaces/DatabaseStatementRAII.h
    # SQLite implementations (always available)
    sqlite/StorageHandlerSQLite.h
    sqlite/StorageHandlerSQLite.cpp
    sqlite/IOTransactionSQLite.h
    sqlite/IOTransactionSQLite.cpp
    sqlite/CommunicatorStorageHandlerSQLite.h
    sqlite/CommunicatorStorageHandlerSQLite.cpp
    sqlite/CommunicatorIOTransactionSQLite.h
    sqlite/CommunicatorIOTransactionSQLite.cpp
    sqlite/TrustLineHandlerSQLite.h
    sqlite/TrustLineHandlerSQLite.cpp
    sqlite/TransactionsHandlerSQLite.h
    sqlite/TransactionsHandlerSQLite.cpp
    sqlite/HistoryStorageSQLite.h
    sqlite/HistoryStorageSQLite.cpp
    sqlite/CommunicatorMessagesQueueHandlerSQLite.h
    sqlite/CommunicatorMessagesQueueHandlerSQLite.cpp
    sqlite/OwnKeysHandlerSQLite.h
    sqlite/OwnKeysHandlerSQLite.cpp
    sqlite/ContractorKeysHandlerSQLite.h
    sqlite/ContractorKeysHandlerSQLite.cpp
    sqlite/AuditHandlerSQLite.h
    sqlite/AuditHandlerSQLite.cpp
    sqlite/PaymentKeysHandlerSQLite.h
    sqlite/PaymentKeysHandlerSQLite.cpp
    sqlite/IncomingPaymentReceiptHandlerSQLite.h
    sqlite/IncomingPaymentReceiptHandlerSQLite.cpp
    sqlite/OutgoingPaymentReceiptHandlerSQLite.h
    sqlite/OutgoingPaymentReceiptHandlerSQLite.cpp
    sqlite/PaymentTransactionsHandlerSQLite.h
    sqlite/PaymentTransactionsHandlerSQLite.cpp
    sqlite/PaymentParticipantsVotesHandlerSQLite.h
    sqlite/PaymentParticipantsVotesHandlerSQLite.cpp
    sqlite/AuditRulesHandlerSQLite.h
    sqlite/AuditRulesHandlerSQLite.cpp
    sqlite/ContractorsHandlerSQLite.h
    sqlite/ContractorsHandlerSQLite.cpp
    sqlite/AddressHandlerSQLite.h
    sqlite/AddressHandlerSQLite.cpp
    sqlite/FeaturesHandlerSQLite.h
    sqlite/FeaturesHandlerSQLite.cpp
    sqlite/SQLiteStatementRAII.h
    sqlite/SQLiteStatementRAII.cpp
    )

# Add PostgreSQL implementation sources (if libpq is available)
add_subdirectory(postgresql)

target_include_directories(io__storage
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(io__storage
    PRIVATE
        cxx_std_20
)

# Always link SQLite (required)
target_link_libraries(io__storage
    PUBLIC
        SQLite::SQLite3
        contractors
        settings)

# Add PostgreSQL support if available
if(PostgreSQL_FOUND)
    target_compile_definitions(io__storage 
        PRIVATE 
            POSTGRESQL_PROVIDER_AVAILABLE)
    
    message(STATUS "PostgreSQL found, PostgreSQL provider will be available")
else()
    message(STATUS "PostgreSQL not found, only SQLite provider will be available")
endif()
