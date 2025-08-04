# PostgreSQL storage handlers (placeholder)
# This CMakeLists is intentionally minimal because PostgreSQL logic is not yet
# integrated in the build process. It exists to satisfy directory conventions
# for future expansion while keeping all task-related changes self-contained.

add_library(io__storage_postgresql INTERFACE)

target_include_directories(io__storage_postgresql
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Re-check PostgreSQL
find_package(PostgreSQL QUIET)

if(PostgreSQL_FOUND)
    target_sources(io__storage
        PRIVATE
            AddressHandlerPostgreSQL.h
            AddressHandlerPostgreSQL.cpp
            AuditHandlerPostgreSQL.h
            AuditHandlerPostgreSQL.cpp
            AuditRulesHandlerPostgreSQL.h
            AuditRulesHandlerPostgreSQL.cpp
            ContractorKeysHandlerPostgreSQL.h
            ContractorKeysHandlerPostgreSQL.cpp
            ContractorsHandlerPostgreSQL.h
            ContractorsHandlerPostgreSQL.cpp
            FeaturesHandlerPostgreSQL.h
            FeaturesHandlerPostgreSQL.cpp
            IncomingPaymentReceiptHandlerPostgreSQL.h
            IncomingPaymentReceiptHandlerPostgreSQL.cpp
            OutgoingPaymentReceiptHandlerPostgreSQL.h
            OutgoingPaymentReceiptHandlerPostgreSQL.cpp
            OwnKeysHandlerPostgreSQL.h
            OwnKeysHandlerPostgreSQL.cpp
            PaymentKeysHandlerPostgreSQL.h
            PaymentKeysHandlerPostgreSQL.cpp
            PaymentParticipantsVotesHandlerPostgreSQL.h
            PaymentParticipantsVotesHandlerPostgreSQL.cpp
            PaymentTransactionsHandlerPostgreSQL.h
            PaymentTransactionsHandlerPostgreSQL.cpp
            TransactionsHandlerPostgreSQL.h
            TransactionsHandlerPostgreSQL.cpp
            TrustLineHandlerPostgreSQL.h
            TrustLineHandlerPostgreSQL.cpp
            HistoryStoragePostgreSQL.h
            HistoryStoragePostgreSQL.cpp
            IOTransactionPostgreSQL.h
            IOTransactionPostgreSQL.cpp
            StorageHandlerPostgreSQL.h
            StorageHandlerPostgreSQL.cpp
            CommunicatorMessagesQueueHandlerPostgreSQL.h
            CommunicatorMessagesQueueHandlerPostgreSQL.cpp
            CommunicatorIOTransactionPostgreSQL.h
            CommunicatorIOTransactionPostgreSQL.cpp
            CommunicatorStorageHandlerPostgreSQL.h
            CommunicatorStorageHandlerPostgreSQL.cpp)

    target_link_libraries(io__storage
        PUBLIC
            PostgreSQL::PostgreSQL)
endif() 