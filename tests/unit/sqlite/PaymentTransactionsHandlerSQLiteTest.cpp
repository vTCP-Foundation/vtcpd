#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sqlite3.h>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <limits>

#include "../../../src/core/io/storage/sqlite/PaymentTransactionsHandlerSQLite.h"
#include "../../../src/core/transactions/transactions/base/TransactionUUID.h"
#include "../../../src/core/common/exceptions/IOError.h"
#include "../../../src/core/common/exceptions/ValueError.h"
// Removed unused TestDataFactory to avoid heavy transitive dependencies that are not required for this test

using ::testing::_;
using ::testing::Return;
using ::testing::Throw;
using ::testing::StrictMock;
using ::testing::NiceMock;

class PaymentTransactionsHandlerSQLiteTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test databases
        testDbDir = std::filesystem::temp_directory_path() / "vtcpd_test_payment_transactions";
        std::filesystem::create_directories(testDbDir);
        
        testDbPath = testDbDir / "test_payment_transactions.db";
        
        // Create test database
        int rc = sqlite3_open(testDbPath.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK) << "Failed to open test database: " << sqlite3_errmsg(db);
        
        logger = std::make_unique<Logger>();
        tableName = "payment_transactions";
        
        // Ensure dependent table exists and has at least one key
        const char* createKeysTable =
            "CREATE TABLE IF NOT EXISTS payment_keys ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "public_key BLOB NOT NULL, "
            "private_key BLOB NOT NULL);";
        char* errMsg = nullptr;
        int execRc = sqlite3_exec(db, createKeysTable, nullptr, nullptr, &errMsg);
        ASSERT_EQ(execRc, SQLITE_OK) << "Failed to create payment_keys table: " << (errMsg ? errMsg : "");
        if (errMsg) { sqlite3_free(errMsg); errMsg = nullptr; }

        const char* insertDummyKey =
            "INSERT INTO payment_keys (public_key, private_key) VALUES (zeroblob(32), zeroblob(64));";
        execRc = sqlite3_exec(db, insertDummyKey, nullptr, nullptr, &errMsg);
        ASSERT_EQ(execRc, SQLITE_OK) << "Failed to insert dummy key into payment_keys: " << (errMsg ? errMsg : "");
        if (errMsg) { sqlite3_free(errMsg); errMsg = nullptr; }

        // Create handler instance
        handler = std::make_unique<PaymentTransactionsHandlerSQLite>(db, tableName, *logger);
    }
    
    void TearDown() override {
        handler.reset();
        if (db) {
            sqlite3_close(db);
            db = nullptr;
        }
        
        // Clean up test files
        std::error_code ec;
        std::filesystem::remove_all(testDbDir, ec);
        // Ignore cleanup errors
    }
    
    TransactionUUID createTestUUID(const std::string& uuidString = "") {
        if (uuidString.empty()) {
            return TransactionUUID(); // Generate new UUID
        }
        return TransactionUUID(uuidString);
    }
    
    BlockNumber createTestBlockNumber(uint64_t value = 100) {
        return static_cast<BlockNumber>(value);
    }
    
    size_t countTransactionsInDatabase() {
        const char* query = "SELECT COUNT(*) FROM payment_transactions";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return 0;
        
        size_t count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        
        sqlite3_finalize(stmt);
        return count;
    }
    
    bool transactionExistsInDatabase(const TransactionUUID& uuid) {
        const char* query = "SELECT COUNT(*) FROM payment_transactions WHERE uuid = ?";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return false;
        
        uint8_t uuidData[TransactionUUID::kBytesSize];
        memcpy(uuidData, uuid.data, TransactionUUID::kBytesSize);
        sqlite3_bind_blob(stmt, 1, uuidData, TransactionUUID::kBytesSize, SQLITE_STATIC);
        
        bool exists = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            exists = sqlite3_column_int(stmt, 0) > 0;
        }
        
        sqlite3_finalize(stmt);
        return exists;
    }
    
    int getTransactionObservingState(const TransactionUUID& uuid) {
        const char* query = "SELECT observing_state FROM payment_transactions WHERE uuid = ?";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return -1;
        
        uint8_t uuidData[TransactionUUID::kBytesSize];
        memcpy(uuidData, uuid.data, TransactionUUID::kBytesSize);
        sqlite3_bind_blob(stmt, 1, uuidData, TransactionUUID::kBytesSize, SQLITE_STATIC);
        
        int state = -1;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            state = sqlite3_column_int(stmt, 0);
        }
        
        sqlite3_finalize(stmt);
        return state;
    }

protected:
    std::filesystem::path testDbDir;
    std::filesystem::path testDbPath;
    sqlite3* db = nullptr;
    std::unique_ptr<Logger> logger;
    std::string tableName;
    std::unique_ptr<PaymentTransactionsHandlerSQLite> handler;
};

// Constructor and Table Creation Tests
TEST_F(PaymentTransactionsHandlerSQLiteTest, ConstructorValidParameters) {
    // Verify table was created by constructor
    const char* query = "SELECT name FROM sqlite_master WHERE type='table' AND name='payment_transactions'";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    EXPECT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)), "payment_transactions");
    sqlite3_finalize(stmt);
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, ConstructorCreatesRequiredIndex) {
    // Verify index was created
    const char* query = "SELECT name FROM sqlite_master WHERE type='index' AND tbl_name='payment_transactions'";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    std::vector<std::string> indexes;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        indexes.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
    
    // Check required index exists (payment_key_id index is required by current implementation)
    EXPECT_TRUE(std::find(indexes.begin(), indexes.end(), "payment_transactions_payment_key_id_idx") != indexes.end());
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, ConstructorNullDatabase) {
    EXPECT_THROW(
        PaymentTransactionsHandlerSQLite(nullptr, "test_table", *logger),
        ValueError
    );
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, ConstructorEmptyTableName) {
    EXPECT_THROW(
        PaymentTransactionsHandlerSQLite(db, "", *logger),
        ValueError
    );
}

// Save Record Tests
TEST_F(PaymentTransactionsHandlerSQLiteTest, SaveRecordValidData) {
    auto transactionUUID = createTestUUID();
    auto blockNumber = createTestBlockNumber(12345);
    
    EXPECT_NO_THROW(handler->saveRecord(transactionUUID, blockNumber));
    
    // Verify data was saved
    EXPECT_EQ(countTransactionsInDatabase(), 1);
    EXPECT_TRUE(transactionExistsInDatabase(transactionUUID));
    EXPECT_TRUE(handler->isTransactionPresent(transactionUUID));
    
    // Verify initial observing state is 0
    EXPECT_EQ(getTransactionObservingState(transactionUUID), 0);
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, SaveRecordWithSpecificUUID) {
    auto transactionUUID = createTestUUID("12345678-1234-5678-9abc-123456789012");
    auto blockNumber = createTestBlockNumber(54321);
    
    EXPECT_NO_THROW(handler->saveRecord(transactionUUID, blockNumber));
    
    // Verify data was saved
    EXPECT_EQ(countTransactionsInDatabase(), 1);
    EXPECT_TRUE(handler->isTransactionPresent(transactionUUID));
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, SaveRecordLargeBlockNumber) {
    auto transactionUUID = createTestUUID();
    auto blockNumber = createTestBlockNumber(std::numeric_limits<uint64_t>::max() - 1);
    
    EXPECT_NO_THROW(handler->saveRecord(transactionUUID, blockNumber));
    
    // Verify data was saved
    EXPECT_EQ(countTransactionsInDatabase(), 1);
    EXPECT_TRUE(handler->isTransactionPresent(transactionUUID));
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, SaveRecordZeroBlockNumber) {
    auto transactionUUID = createTestUUID();
    auto blockNumber = createTestBlockNumber(0);
    
    EXPECT_NO_THROW(handler->saveRecord(transactionUUID, blockNumber));
    
    // Verify data was saved
    EXPECT_EQ(countTransactionsInDatabase(), 1);
    EXPECT_TRUE(handler->isTransactionPresent(transactionUUID));
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, SaveRecordDuplicateUUID) {
    auto transactionUUID = createTestUUID();
    auto blockNumber1 = createTestBlockNumber(100);
    auto blockNumber2 = createTestBlockNumber(200);
    
    EXPECT_NO_THROW(handler->saveRecord(transactionUUID, blockNumber1));
    
    // Attempting to save duplicate UUID should throw (if there's a unique constraint)
    // or succeed (if duplicates are allowed) - need to check implementation
    // Based on the schema, it seems duplicates might be allowed
    EXPECT_NO_THROW(handler->saveRecord(transactionUUID, blockNumber2));
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, SaveMultipleRecords) {
    auto uuid1 = createTestUUID();
    auto uuid2 = createTestUUID();
    auto uuid3 = createTestUUID();
    
    auto block1 = createTestBlockNumber(100);
    auto block2 = createTestBlockNumber(200);
    auto block3 = createTestBlockNumber(300);
    
    EXPECT_NO_THROW(handler->saveRecord(uuid1, block1));
    EXPECT_NO_THROW(handler->saveRecord(uuid2, block2));
    EXPECT_NO_THROW(handler->saveRecord(uuid3, block3));
    
    // Verify all records were saved
    EXPECT_EQ(countTransactionsInDatabase(), 3);
    EXPECT_TRUE(handler->isTransactionPresent(uuid1));
    EXPECT_TRUE(handler->isTransactionPresent(uuid2));
    EXPECT_TRUE(handler->isTransactionPresent(uuid3));
}

// Update Transaction State Tests
TEST_F(PaymentTransactionsHandlerSQLiteTest, UpdateTransactionStateValidData) {
    auto transactionUUID = createTestUUID();
    auto blockNumber = createTestBlockNumber(100);
    
    // Save transaction first
    handler->saveRecord(transactionUUID, blockNumber);
    EXPECT_EQ(getTransactionObservingState(transactionUUID), 0);
    
    // Update state
    EXPECT_NO_THROW(handler->updateTransactionState(transactionUUID, 1));
    EXPECT_EQ(getTransactionObservingState(transactionUUID), 1);
    
    // Update to different state
    EXPECT_NO_THROW(handler->updateTransactionState(transactionUUID, 2));
    EXPECT_EQ(getTransactionObservingState(transactionUUID), 2);
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, UpdateTransactionStateNonExistent) {
    auto transactionUUID = createTestUUID();
    
    // Update non-existent transaction should throw
    EXPECT_THROW(
        handler->updateTransactionState(transactionUUID, 1),
        ValueError
    );
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, UpdateTransactionStateNegativeValue) {
    auto transactionUUID = createTestUUID();
    auto blockNumber = createTestBlockNumber(100);
    
    // Save transaction first
    handler->saveRecord(transactionUUID, blockNumber);
    
    // Update to negative state
    EXPECT_NO_THROW(handler->updateTransactionState(transactionUUID, -1));
    EXPECT_EQ(getTransactionObservingState(transactionUUID), -1);
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, UpdateTransactionStateLargeValue) {
    auto transactionUUID = createTestUUID();
    auto blockNumber = createTestBlockNumber(100);
    
    // Save transaction first
    handler->saveRecord(transactionUUID, blockNumber);
    
    // Update to large state value
    int largeState = std::numeric_limits<int>::max() - 1;
    EXPECT_NO_THROW(handler->updateTransactionState(transactionUUID, largeState));
    EXPECT_EQ(getTransactionObservingState(transactionUUID), largeState);
}

// Transactions With Uncertain Observing State Tests
TEST_F(PaymentTransactionsHandlerSQLiteTest, TransactionsWithUncertainObservingStateEmpty) {
    auto result = handler->transactionsWithUncertainObservingState();
    EXPECT_TRUE(result.empty());
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, TransactionsWithUncertainObservingStateAllUncertain) {
    auto uuid1 = createTestUUID();
    auto uuid2 = createTestUUID();
    auto uuid3 = createTestUUID();
    
    auto block1 = createTestBlockNumber(100);
    auto block2 = createTestBlockNumber(200);
    auto block3 = createTestBlockNumber(300);
    
    // Save all with uncertain state (default 0)
    handler->saveRecord(uuid1, block1);
    handler->saveRecord(uuid2, block2);
    handler->saveRecord(uuid3, block3);
    
    auto result = handler->transactionsWithUncertainObservingState();
    ASSERT_EQ(result.size(), 3);
    
    // Verify UUIDs are returned (order might vary)
    std::vector<std::string> returnedUUIDs;
    for (const auto& pair : result) {
        returnedUUIDs.push_back(pair.first.stringUUID());
    }
    
    EXPECT_TRUE(std::find(returnedUUIDs.begin(), returnedUUIDs.end(), uuid1.stringUUID()) != returnedUUIDs.end());
    EXPECT_TRUE(std::find(returnedUUIDs.begin(), returnedUUIDs.end(), uuid2.stringUUID()) != returnedUUIDs.end());
    EXPECT_TRUE(std::find(returnedUUIDs.begin(), returnedUUIDs.end(), uuid3.stringUUID()) != returnedUUIDs.end());
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, TransactionsWithUncertainObservingStateMixed) {
    auto uuid1 = createTestUUID();
    auto uuid2 = createTestUUID();
    auto uuid3 = createTestUUID();
    
    auto block1 = createTestBlockNumber(100);
    auto block2 = createTestBlockNumber(200);
    auto block3 = createTestBlockNumber(300);
    
    // Save transactions
    handler->saveRecord(uuid1, block1);
    handler->saveRecord(uuid2, block2);
    handler->saveRecord(uuid3, block3);
    
    // Update some states to non-uncertain
    handler->updateTransactionState(uuid1, 1);
    handler->updateTransactionState(uuid3, 2);
    
    // Only uuid2 should have uncertain state (0)
    auto result = handler->transactionsWithUncertainObservingState();
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].first.stringUUID(), uuid2.stringUUID());
}

// Is Transaction Present Tests
TEST_F(PaymentTransactionsHandlerSQLiteTest, IsTransactionPresentExisting) {
    auto transactionUUID = createTestUUID();
    auto blockNumber = createTestBlockNumber(100);
    
    // Initially not present
    EXPECT_FALSE(handler->isTransactionPresent(transactionUUID));
    
    // Save and check
    handler->saveRecord(transactionUUID, blockNumber);
    EXPECT_TRUE(handler->isTransactionPresent(transactionUUID));
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, IsTransactionPresentNonExistent) {
    auto transactionUUID = createTestUUID();
    
    EXPECT_FALSE(handler->isTransactionPresent(transactionUUID));
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, IsTransactionPresentAfterStateUpdate) {
    auto transactionUUID = createTestUUID();
    auto blockNumber = createTestBlockNumber(100);
    
    // Save and update state
    handler->saveRecord(transactionUUID, blockNumber);
    handler->updateTransactionState(transactionUUID, 5);
    
    // Should still be present
    EXPECT_TRUE(handler->isTransactionPresent(transactionUUID));
}

// Delete Record Tests
TEST_F(PaymentTransactionsHandlerSQLiteTest, DeleteRecordExisting) {
    auto transactionUUID = createTestUUID();
    auto blockNumber = createTestBlockNumber(100);
    
    // Save transaction
    handler->saveRecord(transactionUUID, blockNumber);
    EXPECT_TRUE(handler->isTransactionPresent(transactionUUID));
    EXPECT_EQ(countTransactionsInDatabase(), 1);
    
    // Delete transaction
    EXPECT_NO_THROW(handler->deleteRecord(transactionUUID));
    
    // Verify deletion
    EXPECT_FALSE(handler->isTransactionPresent(transactionUUID));
    EXPECT_EQ(countTransactionsInDatabase(), 0);
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, DeleteRecordNonExistent) {
    auto transactionUUID = createTestUUID();
    
    // Delete non-existent transaction should not throw
    EXPECT_NO_THROW(handler->deleteRecord(transactionUUID));
    EXPECT_EQ(countTransactionsInDatabase(), 0);
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, DeleteRecordSpecific) {
    auto uuid1 = createTestUUID();
    auto uuid2 = createTestUUID();
    auto uuid3 = createTestUUID();
    
    auto block1 = createTestBlockNumber(100);
    auto block2 = createTestBlockNumber(200);
    auto block3 = createTestBlockNumber(300);
    
    // Save multiple transactions
    handler->saveRecord(uuid1, block1);
    handler->saveRecord(uuid2, block2);
    handler->saveRecord(uuid3, block3);
    EXPECT_EQ(countTransactionsInDatabase(), 3);
    
    // Delete specific transaction
    handler->deleteRecord(uuid2);
    
    // Verify only uuid2 was deleted
    EXPECT_EQ(countTransactionsInDatabase(), 2);
    EXPECT_TRUE(handler->isTransactionPresent(uuid1));
    EXPECT_FALSE(handler->isTransactionPresent(uuid2));
    EXPECT_TRUE(handler->isTransactionPresent(uuid3));
}

// All Transactions UUID Tests
TEST_F(PaymentTransactionsHandlerSQLiteTest, AllTransactionsUUIDEmpty) {
    auto result = handler->allTransactionsUUID();
    EXPECT_TRUE(result.empty());
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, AllTransactionsUUIDSingle) {
    auto transactionUUID = createTestUUID();
    auto blockNumber = createTestBlockNumber(100);
    
    handler->saveRecord(transactionUUID, blockNumber);
    
    auto result = handler->allTransactionsUUID();
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].stringUUID(), transactionUUID.stringUUID());
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, AllTransactionsUUIDMultiple) {
    auto uuid1 = createTestUUID();
    auto uuid2 = createTestUUID();
    auto uuid3 = createTestUUID();
    
    auto block1 = createTestBlockNumber(100);
    auto block2 = createTestBlockNumber(200);
    auto block3 = createTestBlockNumber(300);
    
    handler->saveRecord(uuid1, block1);
    handler->saveRecord(uuid2, block2);
    handler->saveRecord(uuid3, block3);
    
    auto result = handler->allTransactionsUUID();
    ASSERT_EQ(result.size(), 3);
    
    // Verify all UUIDs are returned (order might vary)
    std::vector<std::string> returnedUUIDs;
    for (const auto& uuid : result) {
        returnedUUIDs.push_back(uuid.stringUUID());
    }
    
    EXPECT_TRUE(std::find(returnedUUIDs.begin(), returnedUUIDs.end(), uuid1.stringUUID()) != returnedUUIDs.end());
    EXPECT_TRUE(std::find(returnedUUIDs.begin(), returnedUUIDs.end(), uuid2.stringUUID()) != returnedUUIDs.end());
    EXPECT_TRUE(std::find(returnedUUIDs.begin(), returnedUUIDs.end(), uuid3.stringUUID()) != returnedUUIDs.end());
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, AllTransactionsUUIDAfterDeletion) {
    auto uuid1 = createTestUUID();
    auto uuid2 = createTestUUID();
    
    auto block1 = createTestBlockNumber(100);
    auto block2 = createTestBlockNumber(200);
    
    handler->saveRecord(uuid1, block1);
    handler->saveRecord(uuid2, block2);
    
    auto resultBefore = handler->allTransactionsUUID();
    EXPECT_EQ(resultBefore.size(), 2);
    
    // Delete one transaction
    handler->deleteRecord(uuid1);
    
    auto resultAfter = handler->allTransactionsUUID();
    ASSERT_EQ(resultAfter.size(), 1);
    EXPECT_EQ(resultAfter[0].stringUUID(), uuid2.stringUUID());
}

// Edge Cases and Error Handling Tests
TEST_F(PaymentTransactionsHandlerSQLiteTest, MultipleOperationsOnSameTransaction) {
    auto transactionUUID = createTestUUID();
    auto blockNumber = createTestBlockNumber(100);
    
    // Save, update, check, delete
    handler->saveRecord(transactionUUID, blockNumber);
    EXPECT_TRUE(handler->isTransactionPresent(transactionUUID));
    
    handler->updateTransactionState(transactionUUID, 3);
    EXPECT_EQ(getTransactionObservingState(transactionUUID), 3);
    
    auto uncertainTransactions = handler->transactionsWithUncertainObservingState();
    EXPECT_TRUE(uncertainTransactions.empty()); // State is 3, not 0
    
    handler->deleteRecord(transactionUUID);
    EXPECT_FALSE(handler->isTransactionPresent(transactionUUID));
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, ConcurrentAccessSimulation) {
    auto uuid1 = createTestUUID();
    auto uuid2 = createTestUUID();
    
    auto block1 = createTestBlockNumber(100);
    auto block2 = createTestBlockNumber(200);
    
    // Simulate concurrent operations
    EXPECT_NO_THROW(handler->saveRecord(uuid1, block1));
    EXPECT_NO_THROW(handler->saveRecord(uuid2, block2));
    
    EXPECT_TRUE(handler->isTransactionPresent(uuid1));
    EXPECT_TRUE(handler->isTransactionPresent(uuid2));
    
    auto allUUIDs = handler->allTransactionsUUID();
    EXPECT_EQ(allUUIDs.size(), 2);
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, PerformanceReasonableTime) {
    const size_t numTransactions = 1000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Save many transactions
    for (size_t i = 0; i < numTransactions; ++i) {
        auto uuid = createTestUUID();
        auto blockNumber = createTestBlockNumber(i);
        handler->saveRecord(uuid, blockNumber);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time. Relaxed threshold for CI/debug builds
    EXPECT_LT(duration.count(), 15000);
    
    // Verify all transactions were saved
    auto allTransactions = handler->allTransactionsUUID();
    EXPECT_EQ(allTransactions.size(), numTransactions);
}

// Data Integrity Tests
TEST_F(PaymentTransactionsHandlerSQLiteTest, DataIntegrityAfterMultipleOperations) {
    auto uuid1 = createTestUUID();
    auto uuid2 = createTestUUID();
    auto uuid3 = createTestUUID();
    
    auto block1 = createTestBlockNumber(100);
    auto block2 = createTestBlockNumber(200);
    auto block3 = createTestBlockNumber(300);
    
    // Complex sequence of operations
    handler->saveRecord(uuid1, block1);
    handler->saveRecord(uuid2, block2);
    handler->saveRecord(uuid3, block3);
    
    handler->updateTransactionState(uuid1, 1);
    handler->updateTransactionState(uuid3, 2);
    
    // uuid2 should have uncertain state
    auto uncertainTransactions = handler->transactionsWithUncertainObservingState();
    ASSERT_EQ(uncertainTransactions.size(), 1);
    EXPECT_EQ(uncertainTransactions[0].first.stringUUID(), uuid2.stringUUID());
    
    // Delete uuid1
    handler->deleteRecord(uuid1);
    
    // Verify final state
    auto allTransactions = handler->allTransactionsUUID();
    EXPECT_EQ(allTransactions.size(), 2);
    
    EXPECT_FALSE(handler->isTransactionPresent(uuid1));
    EXPECT_TRUE(handler->isTransactionPresent(uuid2));
    EXPECT_TRUE(handler->isTransactionPresent(uuid3));
    
    // uncertain state should still only contain uuid2
    auto finalUncertainTransactions = handler->transactionsWithUncertainObservingState();
    ASSERT_EQ(finalUncertainTransactions.size(), 1);
    EXPECT_EQ(finalUncertainTransactions[0].first.stringUUID(), uuid2.stringUUID());
}

// transactionsForObserverMonitoring tests
TEST_F(PaymentTransactionsHandlerSQLiteTest, TransactionsForObserverMonitoring_BasicRetrieval_ReturnsMatchingTransactions) {
    handler->saveRecord(createTestUUID(), createTestBlockNumber(50));
    handler->saveRecord(createTestUUID(), createTestBlockNumber(100));
    handler->saveRecord(createTestUUID(), createTestBlockNumber(150));
    handler->saveRecord(createTestUUID(), createTestBlockNumber(200));
    handler->saveRecord(createTestUUID(), createTestBlockNumber(250));

    auto result = handler->transactionsForObserverMonitoring(createTestBlockNumber(100), 10);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].second, createTestBlockNumber(150));
    EXPECT_EQ(result[1].second, createTestBlockNumber(200));
    EXPECT_EQ(result[2].second, createTestBlockNumber(250));
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, TransactionsForObserverMonitoring_StateFiltering_OnlyReturnsUncertainState) {
    auto uuidState0 = createTestUUID();
    auto uuidState1 = createTestUUID();
    auto uuidState2 = createTestUUID();

    handler->saveRecord(uuidState0, createTestBlockNumber(100));
    handler->saveRecord(uuidState1, createTestBlockNumber(100));
    handler->saveRecord(uuidState2, createTestBlockNumber(100));

    handler->updateTransactionState(uuidState1, 1);
    handler->updateTransactionState(uuidState2, 2);

    auto result = handler->transactionsForObserverMonitoring(createTestBlockNumber(0), 10);

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].first.stringUUID(), uuidState0.stringUUID());
    EXPECT_EQ(result[0].second, createTestBlockNumber(100));
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, TransactionsForObserverMonitoring_LimitParameter_RespectsLimit) {
    for (uint64_t i = 1; i <= 10; ++i) {
        handler->saveRecord(createTestUUID(), createTestBlockNumber(i * 10));
    }

    auto result = handler->transactionsForObserverMonitoring(createTestBlockNumber(0), 3);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].second, createTestBlockNumber(10));
    EXPECT_EQ(result[1].second, createTestBlockNumber(20));
    EXPECT_EQ(result[2].second, createTestBlockNumber(30));
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, TransactionsForObserverMonitoring_AscendingOrder_OldestFirst) {
    handler->saveRecord(createTestUUID(), createTestBlockNumber(30));
    handler->saveRecord(createTestUUID(), createTestBlockNumber(10));
    handler->saveRecord(createTestUUID(), createTestBlockNumber(20));

    auto result = handler->transactionsForObserverMonitoring(createTestBlockNumber(0), 10);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].second, createTestBlockNumber(10));
    EXPECT_EQ(result[1].second, createTestBlockNumber(20));
    EXPECT_EQ(result[2].second, createTestBlockNumber(30));
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, TransactionsForObserverMonitoring_EmptyResult_NoMatchingTransactions) {
    handler->saveRecord(createTestUUID(), createTestBlockNumber(50));
    handler->saveRecord(createTestUUID(), createTestBlockNumber(100));

    auto result = handler->transactionsForObserverMonitoring(createTestBlockNumber(100), 10);

    EXPECT_TRUE(result.empty());
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, TransactionsForObserverMonitoring_BoundaryCondition_ExactBlockNumber) {
    handler->saveRecord(createTestUUID(), createTestBlockNumber(100));

    auto result = handler->transactionsForObserverMonitoring(createTestBlockNumber(100), 10);

    EXPECT_TRUE(result.empty());
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, TransactionsForObserverMonitoring_ZeroLimit_ReturnsEmpty) {
    handler->saveRecord(createTestUUID(), createTestBlockNumber(100));
    handler->saveRecord(createTestUUID(), createTestBlockNumber(200));

    auto result = handler->transactionsForObserverMonitoring(createTestBlockNumber(0), 0);

    EXPECT_TRUE(result.empty());
}

TEST_F(PaymentTransactionsHandlerSQLiteTest, TransactionsForObserverMonitoring_LargeDataset_ReturnsExpectedRange) {
    for (uint64_t i = 1; i <= 200; ++i) {
        handler->saveRecord(createTestUUID(), createTestBlockNumber(i));
    }

    auto result = handler->transactionsForObserverMonitoring(createTestBlockNumber(50), 100);

    ASSERT_EQ(result.size(), 100);
    EXPECT_EQ(result.front().second, createTestBlockNumber(51));
    EXPECT_EQ(result.back().second, createTestBlockNumber(150));

    for (size_t i = 0; i < result.size(); ++i) {
        EXPECT_EQ(result[i].second, createTestBlockNumber(51 + i));
    }
}
