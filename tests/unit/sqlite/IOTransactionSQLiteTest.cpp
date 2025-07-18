#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sqlite3.h>
#include <memory>
#include <string>
#include <sstream>
#include <filesystem>
#include <chrono>

#include "../../../src/core/io/storage/sqlite/IOTransactionSQLite.h"
#include "../../../src/core/io/storage/sqlite/TrustLineHandlerSQLite.h"
#include "../../../src/core/io/storage/sqlite/HistoryStorageSQLite.h"
#include "../../../src/core/io/storage/sqlite/TransactionsHandlerSQLite.h"
#include "../../../src/core/io/storage/sqlite/OwnKeysHandlerSQLite.h"
#include "../../../src/core/io/storage/sqlite/ContractorKeysHandlerSQLite.h"
#include "../../../src/core/io/storage/sqlite/AuditHandlerSQLite.h"
#include "../../../src/core/io/storage/sqlite/IncomingPaymentReceiptHandlerSQLite.h"
#include "../../../src/core/io/storage/sqlite/OutgoingPaymentReceiptHandlerSQLite.h"
#include "../../../src/core/io/storage/sqlite/PaymentKeysHandlerSQLite.h"
#include "../../../src/core/io/storage/sqlite/PaymentParticipantsVotesHandlerSQLite.h"
#include "../../../src/core/io/storage/sqlite/PaymentTransactionsHandlerSQLite.h"
#include "../../../src/core/io/storage/sqlite/ContractorsHandlerSQLite.h"
#include "../../../src/core/io/storage/sqlite/AddressHandlerSQLite.h"
#include "../../../src/core/io/storage/sqlite/FeaturesHandlerSQLite.h"
#include "../../../src/core/common/exceptions/IOError.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/contractors/Contractor.h"
#include "../../../src/core/contractors/addresses/IPv4WithPortAddress.h"
#include "../../../src/core/crypto/MsgEncryptor.h"

using namespace std;
using namespace testing;

// Minimal dummy factory for Contractor objects
class DummyFactory {
public:
    Contractor::Shared generateContractor() {
        static ContractorID sID = 1;
        vector<BaseAddress::Shared> addresses;
        addresses.push_back(make_shared<IPv4WithPortAddress>("127.0.0.1:4040"));
        auto keyTrio = MsgEncryptor::generateKeyTrio();
        return make_shared<Contractor>(sID++, addresses, keyTrio);
    }
};

class IOTransactionSQLiteTest : public Test {
protected:
    void SetUp() override {
        // Create temporary directory for test database
        tempDir = filesystem::temp_directory_path() / "io_transaction_test";
        filesystem::create_directories(tempDir);
        
        // Create test database
        testDbPath = tempDir / "test.db";
        int rc = sqlite3_open(testDbPath.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK);
        
        // Create Logger
        logger = make_unique<Logger>();
        
        // Create dummy data factory
        dummyFactory = make_unique<DummyFactory>();
        
        // Create all handlers
        trustLineHandler = make_unique<TrustLineHandlerSQLite>(db, "trust_lines", *logger);
        historyStorage = make_unique<HistoryStorageSQLite>(db, "history_main", "history_additional", *logger);
        transactionsHandler = make_unique<TransactionsHandlerSQLite>(db, "transactions", *logger);
        ownKeysHandler = make_unique<OwnKeysHandlerSQLite>(db, "own_keys", *logger);
        contractorKeysHandler = make_unique<ContractorKeysHandlerSQLite>(db, "contractor_keys", *logger);
        auditHandler = make_unique<AuditHandlerSQLite>(db, "audit", *logger);
        incomingReceiptHandler = make_unique<IncomingPaymentReceiptHandlerSQLite>(db, "incoming_receipts", *logger);
        outgoingReceiptHandler = make_unique<OutgoingPaymentReceiptHandlerSQLite>(db, "outgoing_receipts", *logger);
        paymentKeysHandler = make_unique<PaymentKeysHandlerSQLite>(db, "payment_keys", *logger);
        paymentVotesHandler = make_unique<PaymentParticipantsVotesHandlerSQLite>(db, "payment_votes", *logger);
        paymentTransactionsHandler = make_unique<PaymentTransactionsHandlerSQLite>(db, "payment_transactions", *logger);
        contractorsHandler = make_unique<ContractorsHandlerSQLite>(db, "contractors", *logger);
        addressHandler = make_unique<AddressHandlerSQLite>(db, "addresses", *logger);
        featuresHandler = make_unique<FeaturesHandlerSQLite>(db, "features", *logger);
        
        // Create IOTransactionSQLite
        ioTransaction = make_unique<IOTransactionSQLite>(
            db,
            trustLineHandler.get(),
            historyStorage.get(),
            transactionsHandler.get(),
            ownKeysHandler.get(),
            contractorKeysHandler.get(),
            auditHandler.get(),
            incomingReceiptHandler.get(),
            outgoingReceiptHandler.get(),
            paymentKeysHandler.get(),
            paymentVotesHandler.get(),
            paymentTransactionsHandler.get(),
            contractorsHandler.get(),
            addressHandler.get(),
            featuresHandler.get(),
            *logger
        );
    }
    
    void TearDown() override {
        ioTransaction.reset();
        
        // Clean up handlers
        featuresHandler.reset();
        addressHandler.reset();
        contractorsHandler.reset();
        paymentTransactionsHandler.reset();
        paymentVotesHandler.reset();
        paymentKeysHandler.reset();
        outgoingReceiptHandler.reset();
        incomingReceiptHandler.reset();
        auditHandler.reset();
        contractorKeysHandler.reset();
        ownKeysHandler.reset();
        transactionsHandler.reset();
        historyStorage.reset();
        trustLineHandler.reset();
        
        logger.reset();
        dummyFactory.reset();
        
        if (db) {
            sqlite3_close(db);
        }
        
        filesystem::remove_all(tempDir);
    }
    
    filesystem::path tempDir;
    filesystem::path testDbPath;
    sqlite3* db = nullptr;
    unique_ptr<Logger> logger;
    unique_ptr<DummyFactory> dummyFactory;
    
    // Handlers
    unique_ptr<TrustLineHandlerSQLite> trustLineHandler;
    unique_ptr<HistoryStorageSQLite> historyStorage;
    unique_ptr<TransactionsHandlerSQLite> transactionsHandler;
    unique_ptr<OwnKeysHandlerSQLite> ownKeysHandler;
    unique_ptr<ContractorKeysHandlerSQLite> contractorKeysHandler;
    unique_ptr<AuditHandlerSQLite> auditHandler;
    unique_ptr<IncomingPaymentReceiptHandlerSQLite> incomingReceiptHandler;
    unique_ptr<OutgoingPaymentReceiptHandlerSQLite> outgoingReceiptHandler;
    unique_ptr<PaymentKeysHandlerSQLite> paymentKeysHandler;
    unique_ptr<PaymentParticipantsVotesHandlerSQLite> paymentVotesHandler;
    unique_ptr<PaymentTransactionsHandlerSQLite> paymentTransactionsHandler;
    unique_ptr<ContractorsHandlerSQLite> contractorsHandler;
    unique_ptr<AddressHandlerSQLite> addressHandler;
    unique_ptr<FeaturesHandlerSQLite> featuresHandler;
    
    unique_ptr<IOTransactionSQLite> ioTransaction;
};

// Constructor Tests
TEST_F(IOTransactionSQLiteTest, Constructor_ValidParameters_CreatesSuccessfully) {
    EXPECT_NE(ioTransaction, nullptr);
}

TEST_F(IOTransactionSQLiteTest, Constructor_NullDatabase_ThrowsException) {
    EXPECT_THROW(
        IOTransactionSQLite(
            nullptr,
            trustLineHandler.get(),
            historyStorage.get(),
            transactionsHandler.get(),
            ownKeysHandler.get(),
            contractorKeysHandler.get(),
            auditHandler.get(),
            incomingReceiptHandler.get(),
            outgoingReceiptHandler.get(),
            paymentKeysHandler.get(),
            paymentVotesHandler.get(),
            paymentTransactionsHandler.get(),
            contractorsHandler.get(),
            addressHandler.get(),
            featuresHandler.get(),
            *logger
        ),
        std::exception
    );
}

TEST_F(IOTransactionSQLiteTest, Constructor_NullTrustLineHandler_ThrowsException) {
    EXPECT_THROW(
        IOTransactionSQLite(
            db,
            nullptr,
            historyStorage.get(),
            transactionsHandler.get(),
            ownKeysHandler.get(),
            contractorKeysHandler.get(),
            auditHandler.get(),
            incomingReceiptHandler.get(),
            outgoingReceiptHandler.get(),
            paymentKeysHandler.get(),
            paymentVotesHandler.get(),
            paymentTransactionsHandler.get(),
            contractorsHandler.get(),
            addressHandler.get(),
            featuresHandler.get(),
            *logger
        ),
        std::exception
    );
}

// Handler Access Tests
TEST_F(IOTransactionSQLiteTest, TrustLinesHandler_ReturnsCorrectHandler) {
    TrustLineHandler* handler = ioTransaction->trustLinesHandler();
    EXPECT_EQ(handler, trustLineHandler.get());
}

TEST_F(IOTransactionSQLiteTest, HistoryStorage_ReturnsCorrectHandler) {
    HistoryStorage* handler = ioTransaction->historyStorage();
    EXPECT_EQ(handler, historyStorage.get());
}

TEST_F(IOTransactionSQLiteTest, TransactionHandler_ReturnsCorrectHandler) {
    TransactionsHandler* handler = ioTransaction->transactionHandler();
    EXPECT_EQ(handler, transactionsHandler.get());
}

TEST_F(IOTransactionSQLiteTest, OwnKeysHandler_ReturnsCorrectHandler) {
    OwnKeysHandler* handler = ioTransaction->ownKeysHandler();
    EXPECT_EQ(handler, ownKeysHandler.get());
}

TEST_F(IOTransactionSQLiteTest, ContractorKeysHandler_ReturnsCorrectHandler) {
    ContractorKeysHandler* handler = ioTransaction->contractorKeysHandler();
    EXPECT_EQ(handler, contractorKeysHandler.get());
}

TEST_F(IOTransactionSQLiteTest, AuditHandler_ReturnsCorrectHandler) {
    AuditHandler* handler = ioTransaction->auditHandler();
    EXPECT_EQ(handler, auditHandler.get());
}

TEST_F(IOTransactionSQLiteTest, IncomingPaymentReceiptHandler_ReturnsCorrectHandler) {
    IncomingPaymentReceiptHandler* handler = ioTransaction->incomingPaymentReceiptHandler();
    EXPECT_EQ(handler, incomingReceiptHandler.get());
}

TEST_F(IOTransactionSQLiteTest, OutgoingPaymentReceiptHandler_ReturnsCorrectHandler) {
    OutgoingPaymentReceiptHandler* handler = ioTransaction->outgoingPaymentReceiptHandler();
    EXPECT_EQ(handler, outgoingReceiptHandler.get());
}

TEST_F(IOTransactionSQLiteTest, PaymentKeysHandler_ReturnsCorrectHandler) {
    PaymentKeysHandler* handler = ioTransaction->paymentKeysHandler();
    EXPECT_EQ(handler, paymentKeysHandler.get());
}

TEST_F(IOTransactionSQLiteTest, PaymentParticipantsVotesHandler_ReturnsCorrectHandler) {
    PaymentParticipantsVotesHandler* handler = ioTransaction->paymentParticipantsVotesHandler();
    EXPECT_EQ(handler, paymentVotesHandler.get());
}

TEST_F(IOTransactionSQLiteTest, PaymentTransactionsHandler_ReturnsCorrectHandler) {
    PaymentTransactionsHandler* handler = ioTransaction->paymentTransactionsHandler();
    EXPECT_EQ(handler, paymentTransactionsHandler.get());
}

TEST_F(IOTransactionSQLiteTest, ContractorsHandler_ReturnsCorrectHandler) {
    ContractorsHandler* handler = ioTransaction->contractorsHandler();
    EXPECT_EQ(handler, contractorsHandler.get());
}

TEST_F(IOTransactionSQLiteTest, AddressHandler_ReturnsCorrectHandler) {
    AddressHandler* handler = ioTransaction->addressHandler();
    EXPECT_EQ(handler, addressHandler.get());
}

TEST_F(IOTransactionSQLiteTest, FeaturesHandler_ReturnsCorrectHandler) {
    FeaturesHandler* handler = ioTransaction->featuresHandler();
    EXPECT_EQ(handler, featuresHandler.get());
}

// Transaction Management Tests
TEST_F(IOTransactionSQLiteTest, BeginTransactionQuery_ExecutesSuccessfully) {
    EXPECT_NO_THROW(
        ioTransaction->beginTransactionQuery()
    );
}

TEST_F(IOTransactionSQLiteTest, Commit_WithoutTransaction_DoesNotThrow) {
    EXPECT_NO_THROW(
        ioTransaction->commit()
    );
}

TEST_F(IOTransactionSQLiteTest, Rollback_WithoutTransaction_DoesNotThrow) {
    EXPECT_NO_THROW(
        ioTransaction->rollback()
    );
}

TEST_F(IOTransactionSQLiteTest, BeginCommit_WorksCorrectly) {
    EXPECT_NO_THROW(
        ioTransaction->beginTransactionQuery()
    );
    
    EXPECT_NO_THROW(
        ioTransaction->commit()
    );
}

TEST_F(IOTransactionSQLiteTest, BeginRollback_WorksCorrectly) {
    EXPECT_NO_THROW(
        ioTransaction->beginTransactionQuery()
    );
    
    EXPECT_NO_THROW(
        ioTransaction->rollback()
    );
}

TEST_F(IOTransactionSQLiteTest, MultipleBeginTransactions_WorksCorrectly) {
    EXPECT_NO_THROW(
        ioTransaction->beginTransactionQuery()
    );
    
    // Multiple begin calls should not cause issues
    EXPECT_NO_THROW(
        ioTransaction->beginTransactionQuery()
    );
    
    EXPECT_NO_THROW(
        ioTransaction->commit()
    );
}

// Integration Tests
TEST_F(IOTransactionSQLiteTest, Integration_TransactionWithData_WorksCorrectly) {
    // Begin transaction
    EXPECT_NO_THROW(
        ioTransaction->beginTransactionQuery()
    );
    
    // Use one of the handlers to save data
    SerializedEquivalent equivalent = 1;
    string featureName = "test_feature";
    string featureValue = "test_value";
    
    FeaturesHandler* featuresHandlerPtr = ioTransaction->featuresHandler();
    EXPECT_NO_THROW(
        featuresHandlerPtr->saveFeature(featureName, featureValue)
    );
    
    // Commit transaction
    EXPECT_NO_THROW(
        ioTransaction->commit()
    );
    
    // Verify data was saved
    string retrievedValue = featuresHandlerPtr->getFeature(featureName);
    EXPECT_EQ(retrievedValue, featureValue);
}

TEST_F(IOTransactionSQLiteTest, Integration_TransactionRollback_UndoesChanges) {
    SerializedEquivalent equivalent = 1;
    string featureName = "test_feature";
    string featureValue = "test_value";
    
    FeaturesHandler* featuresHandlerPtr = ioTransaction->featuresHandler();
    
    // Begin transaction
    EXPECT_NO_THROW(
        ioTransaction->beginTransactionQuery()
    );
    
    // Save data
    EXPECT_NO_THROW(
        featuresHandlerPtr->saveFeature(featureName, featureValue)
    );
    
    // Rollback transaction
    EXPECT_NO_THROW(
        ioTransaction->rollback()
    );
    
    // Verify data was not saved
    EXPECT_THROW(
        featuresHandlerPtr->getFeature(featureName),
        NotFoundError
    );
}

TEST_F(IOTransactionSQLiteTest, Integration_MultipleHandlers_WorkTogether) {
    // Begin transaction
    EXPECT_NO_THROW(
        ioTransaction->beginTransactionQuery()
    );
    
    // Use multiple handlers
    SerializedEquivalent equivalent = 1;
    
    // Save feature
    FeaturesHandler* featuresHandlerPtr = ioTransaction->featuresHandler();
    EXPECT_NO_THROW(
        featuresHandlerPtr->saveFeature("feature1", "value1")
    );
    
    // Save contractor
    Contractor::Shared contractor = dummyFactory->generateContractor();
    ContractorsHandler* contractorsHandlerPtr = ioTransaction->contractorsHandler();
    EXPECT_NO_THROW(
        contractorsHandlerPtr->saveContractor(contractor)
    );
    
    // Commit transaction
    EXPECT_NO_THROW(
        ioTransaction->commit()
    );
    
    // Verify both saves worked
    string featureValue = featuresHandlerPtr->getFeature("feature1");
    EXPECT_EQ(featureValue, "value1");
    
    vector<ContractorID> contractorIDs = contractorsHandlerPtr->allIDs();
    EXPECT_FALSE(contractorIDs.empty());
}

// Performance Tests
TEST_F(IOTransactionSQLiteTest, Performance_MultipleTransactions_CompletesInReasonableTime) {
    const int numTransactions = 10;
    
    auto start = chrono::high_resolution_clock::now();
    
    FeaturesHandler* featuresHandlerPtr = ioTransaction->featuresHandler();
    
    for (int i = 0; i < numTransactions; ++i) {
        ioTransaction->beginTransactionQuery();
        
        string featureName = "feature_" + to_string(i);
        string featureValue = "value_" + to_string(i);
        
        featuresHandlerPtr->saveFeature(featureName, featureValue);
        
        ioTransaction->commit();
    }
    
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time (2 seconds for 10 transactions)
    EXPECT_LT(duration.count(), 2000);
}

// Error Handling Tests
TEST_F(IOTransactionSQLiteTest, ErrorHandling_CorruptedDatabase_ThrowsIOError) {
    // Close the database to simulate corruption
    sqlite3_close(db);
    db = nullptr;
    
    // Transaction operations should throw IOError
    EXPECT_THROW(
        ioTransaction->beginTransactionQuery(),
        IOError
    );
    
    EXPECT_THROW(
        ioTransaction->commit(),
        IOError
    );
    
    EXPECT_THROW(
        ioTransaction->rollback(),
        IOError
    );
}

TEST_F(IOTransactionSQLiteTest, ErrorHandling_TransactionAfterDestruction_HandlesSafely) {
    // Create a new transaction and immediately destroy it
    auto tempTransaction = make_unique<IOTransactionSQLite>(
        db,
        trustLineHandler.get(),
        historyStorage.get(),
        transactionsHandler.get(),
        ownKeysHandler.get(),
        contractorKeysHandler.get(),
        auditHandler.get(),
        incomingReceiptHandler.get(),
        outgoingReceiptHandler.get(),
        paymentKeysHandler.get(),
        paymentVotesHandler.get(),
        paymentTransactionsHandler.get(),
        contractorsHandler.get(),
        addressHandler.get(),
        featuresHandler.get(),
        *logger
    );
    
    tempTransaction->beginTransactionQuery();
    
    // Destroy the transaction (destructor should handle cleanup)
    tempTransaction.reset();
    
    // Verify database is still accessible
    EXPECT_NO_THROW(
        ioTransaction->beginTransactionQuery()
    );
} 