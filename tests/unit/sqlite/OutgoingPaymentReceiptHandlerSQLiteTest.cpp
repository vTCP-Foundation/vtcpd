#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sqlite3.h>
#include <memory>
#include <string>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <map>
#include <chrono>

#include "../../../src/core/io/storage/sqlite/OutgoingPaymentReceiptHandlerSQLite.h"
#include "../../../src/core/common/exceptions/IOError.h"
#include "../../../src/core/common/exceptions/NotFoundError.h"
#include "../../../src/core/common/exceptions/ValueError.h"
#include "../../../src/core/crypto/lamportkeys.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/common/Types.h"

using namespace std;
using namespace testing;
using namespace crypto::lamport;

class OutgoingPaymentReceiptHandlerSQLiteTest : public Test {
protected:
    void SetUp() override {
        // Create temporary directory for test database
        tempDir = filesystem::temp_directory_path() / "outgoing_receipt_test";
        filesystem::create_directories(tempDir);
        
        // Create test database
        testDbPath = tempDir / "test.db";
        int rc = sqlite3_open(testDbPath.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK);
        
        // Create Logger
        logger = make_unique<Logger>();
        
        // Create handler
        handler = make_unique<OutgoingPaymentReceiptHandlerSQLite>(
            db, 
            "outgoing_receipts_test", 
            *logger
        );
    }
    
    void TearDown() override {
        handler.reset();
        logger.reset();
        
        if (db) {
            sqlite3_close(db);
        }
        
        filesystem::remove_all(tempDir);
    }
    
    // Helper methods
    KeyHash::Shared generateTestKeyHash() {
        static uint8_t counter = 0;
        byte_t hashData[KeyHash::kBytesSize];
        fill(hashData, hashData + KeyHash::kBytesSize, counter++);
        return make_shared<KeyHash>(hashData);
    }
    
    TransactionUUID generateTestTransactionUUID() {
        return TransactionUUID();
    }
    
    TrustLineAmount generateTestAmount() {
        static TrustLineAmount amount = 1;
        return amount++;
    }
    
    TrustLineID generateTestTrustLineID() {
        static TrustLineID id = 1;
        return id++;
    }
    
    filesystem::path tempDir;
    filesystem::path testDbPath;
    sqlite3* db = nullptr;
    unique_ptr<Logger> logger;
    unique_ptr<OutgoingPaymentReceiptHandlerSQLite> handler;
};

// Constructor Tests
TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, Constructor_ValidParameters_CreatesTableAndIndexes) {
    // Verify table exists
    string query = "SELECT name FROM sqlite_master WHERE type='table' AND name='outgoing_receipts_test';";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt);
    EXPECT_EQ(rc, SQLITE_ROW);
    
    string tableName = (char*)sqlite3_column_text(stmt, 0);
    EXPECT_EQ(tableName, "outgoing_receipts_test");
    
    sqlite3_finalize(stmt);
}

TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, Constructor_NullDatabase_ThrowsException) {
    EXPECT_THROW(
        OutgoingPaymentReceiptHandlerSQLite(nullptr, "test_table", *logger),
        ValueError
    );
}

TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, Constructor_EmptyTableName_ThrowsException) {
    EXPECT_THROW(
        OutgoingPaymentReceiptHandlerSQLite(db, "", *logger),
        ValueError
    );
}

// saveRecord Tests
TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, SaveRecord_ValidParameters_SavesSuccessfully) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    KeyHash::Shared keyHash = generateTestKeyHash();
    TrustLineAmount amount = generateTestAmount();
    
    EXPECT_NO_THROW(
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount)
    );
    
    // Verify record was saved
    bool containsTransaction = handler->isContainsTransaction(transactionUUID);
    EXPECT_TRUE(containsTransaction);
    
    bool containsKeyHash = handler->isContainsKeyHash(keyHash);
    EXPECT_TRUE(containsKeyHash);
}

TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, SaveRecord_NullKeyHash_ThrowsException) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    TrustLineAmount amount = generateTestAmount();
    
    EXPECT_THROW(
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, nullptr, amount),
        ValueError
    );
}

TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, SaveRecord_MultipleRecords_SavesSuccessfully) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    const int numRecords = 5;
    
    vector<TransactionUUID> transactionUUIDs;
    vector<KeyHash::Shared> keyHashes;
    
    for (int i = 0; i < numRecords; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        KeyHash::Shared keyHash = generateTestKeyHash();
        TrustLineAmount amount = generateTestAmount();
        
        transactionUUIDs.push_back(transactionUUID);
        keyHashes.push_back(keyHash);
        
        EXPECT_NO_THROW(
            handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount)
        );
    }
    
    // Verify all records were saved
    size_t count = handler->countReceiptsByNumber(trustLineID, auditNumber);
    EXPECT_EQ(count, numRecords);
}

// auditAmounts Tests
TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, AuditAmounts_WithRecords_ReturnsCorrectAmounts) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    
    map<TransactionUUID, TrustLineAmount> expectedAmounts;
    
    for (int i = 0; i < 3; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        KeyHash::Shared keyHash = generateTestKeyHash();
        TrustLineAmount amount = TrustLineAmount(i + 1);
        
        expectedAmounts[transactionUUID] = amount;
        
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount);
    }
    
    map<TransactionUUID, TrustLineAmount> retrievedAmounts = handler->auditAmounts(trustLineID, auditNumber);
    
    EXPECT_EQ(retrievedAmounts.size(), expectedAmounts.size());
    for (const auto& pair : expectedAmounts) {
        EXPECT_EQ(retrievedAmounts[pair.first], pair.second);
    }
}

TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, AuditAmounts_NoRecords_ReturnsEmptyMap) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    
    map<TransactionUUID, TrustLineAmount> amounts = handler->auditAmounts(trustLineID, auditNumber);
    EXPECT_TRUE(amounts.empty());
}

// receiptsByAuditNumber Tests
TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, ReceiptsByAuditNumber_WithRecords_ReturnsCorrectReceipts) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    const int numRecords = 3;
    
    // Save records
    for (int i = 0; i < numRecords; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        KeyHash::Shared keyHash = generateTestKeyHash();
        TrustLineAmount amount = generateTestAmount();
        
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount);
    }
    
    vector<ReceiptRecord::Shared> receipts = handler->receiptsByAuditNumber(trustLineID, auditNumber);
    EXPECT_EQ(receipts.size(), numRecords);
}

TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, ReceiptsByAuditNumber_NoRecords_ReturnsEmptyVector) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    
    vector<ReceiptRecord::Shared> receipts = handler->receiptsByAuditNumber(trustLineID, auditNumber);
    EXPECT_TRUE(receipts.empty());
}

// receiptsLessEqualThanAuditNumber Tests
TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, ReceiptsLessEqualThanAuditNumber_WithRecords_ReturnsCorrectReceipts) {
    TrustLineID trustLineID = generateTestTrustLineID();
    
    // Save records with different audit numbers
    for (int auditNum = 1; auditNum <= 5; ++auditNum) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        KeyHash::Shared keyHash = generateTestKeyHash();
        TrustLineAmount amount = generateTestAmount();
        
        handler->saveRecord(trustLineID, auditNum, transactionUUID, keyHash, amount);
    }
    
    // Get receipts with audit number <= 3
    vector<ReceiptRecord::Shared> receipts = handler->receiptsLessEqualThanAuditNumber(trustLineID, 3);
    EXPECT_EQ(receipts.size(), 3);
}

// deleteRecords Tests
TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, DeleteRecords_ByTransactionUUID_DeletesSuccessfully) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    KeyHash::Shared keyHash = generateTestKeyHash();
    TrustLineAmount amount = generateTestAmount();
    
    // Save record
    handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount);
    
    // Verify record exists
    EXPECT_TRUE(handler->isContainsTransaction(transactionUUID));
    
    // Delete record
    EXPECT_NO_THROW(
        handler->deleteRecords(transactionUUID)
    );
    
    // Verify record is deleted
    EXPECT_FALSE(handler->isContainsTransaction(transactionUUID));
}

TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, DeleteRecords_ByTrustLineID_DeletesSuccessfully) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    const int numRecords = 3;
    
    // Save multiple records
    for (int i = 0; i < numRecords; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        KeyHash::Shared keyHash = generateTestKeyHash();
        TrustLineAmount amount = generateTestAmount();
        
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount);
    }
    
    // Verify records exist
    size_t countBefore = handler->countReceiptsByNumber(trustLineID, auditNumber);
    EXPECT_EQ(countBefore, numRecords);
    
    // Delete all records for trust line
    EXPECT_NO_THROW(
        handler->deleteRecords(trustLineID)
    );
    
    // Verify records are deleted
    size_t countAfter = handler->countReceiptsByNumber(trustLineID, auditNumber);
    EXPECT_EQ(countAfter, 0);
}

// isContainsKeyHash Tests
TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, IsContainsKeyHash_ExistingKeyHash_ReturnsTrue) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    KeyHash::Shared keyHash = generateTestKeyHash();
    TrustLineAmount amount = generateTestAmount();
    
    // Save record
    handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount);
    
    // Check if key hash exists
    bool contains = handler->isContainsKeyHash(keyHash);
    EXPECT_TRUE(contains);
}

TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, IsContainsKeyHash_NonExistentKeyHash_ReturnsFalse) {
    KeyHash::Shared keyHash = generateTestKeyHash();
    
    bool contains = handler->isContainsKeyHash(keyHash);
    EXPECT_FALSE(contains);
}

// isContainsTransaction Tests
TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, IsContainsTransaction_ExistingTransaction_ReturnsTrue) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    KeyHash::Shared keyHash = generateTestKeyHash();
    TrustLineAmount amount = generateTestAmount();
    
    // Save record
    handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount);
    
    // Check if transaction exists
    bool contains = handler->isContainsTransaction(transactionUUID);
    EXPECT_TRUE(contains);
}

TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, IsContainsTransaction_NonExistentTransaction_ReturnsFalse) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    
    bool contains = handler->isContainsTransaction(transactionUUID);
    EXPECT_FALSE(contains);
}

// countReceiptsByNumber Tests
TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, CountReceiptsByNumber_WithRecords_ReturnsCorrectCount) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    const int numRecords = 5;
    
    // Save multiple records
    for (int i = 0; i < numRecords; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        KeyHash::Shared keyHash = generateTestKeyHash();
        TrustLineAmount amount = generateTestAmount();
        
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount);
    }
    
    size_t count = handler->countReceiptsByNumber(trustLineID, auditNumber);
    EXPECT_EQ(count, numRecords);
}

TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, CountReceiptsByNumber_NoRecords_ReturnsZero) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    
    size_t count = handler->countReceiptsByNumber(trustLineID, auditNumber);
    EXPECT_EQ(count, 0);
}

// Integration Tests
TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, Integration_SaveRetrieveDelete_WorksCorrectly) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    KeyHash::Shared keyHash = generateTestKeyHash();
    TrustLineAmount amount = TrustLineAmount(100);
    
    // Save record
    EXPECT_NO_THROW(
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount)
    );
    
    // Retrieve record
    vector<ReceiptRecord::Shared> receipts = handler->receiptsByAuditNumber(trustLineID, auditNumber);
    EXPECT_EQ(receipts.size(), 1);
    
    // Check amounts
    map<TransactionUUID, TrustLineAmount> amounts = handler->auditAmounts(trustLineID, auditNumber);
    EXPECT_EQ(amounts.size(), 1);
    EXPECT_EQ(amounts[transactionUUID], amount);
    
    // Count receipts
    size_t count = handler->countReceiptsByNumber(trustLineID, auditNumber);
    EXPECT_EQ(count, 1);
    
    // Delete record
    EXPECT_NO_THROW(
        handler->deleteRecords(transactionUUID)
    );
    
    // Verify deletion
    receipts = handler->receiptsByAuditNumber(trustLineID, auditNumber);
    EXPECT_TRUE(receipts.empty());
}

// Performance Tests
TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, Performance_BulkOperations_CompletesInReasonableTime) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    const int numRecords = 100;
    
    auto start = chrono::high_resolution_clock::now();
    
    vector<TransactionUUID> transactionUUIDs;
    
    // Bulk save
    for (int i = 0; i < numRecords; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        KeyHash::Shared keyHash = generateTestKeyHash();
        TrustLineAmount amount = generateTestAmount();
        
        transactionUUIDs.push_back(transactionUUID);
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount);
    }
    
    // Bulk retrieve
    vector<ReceiptRecord::Shared> receipts = handler->receiptsByAuditNumber(trustLineID, auditNumber);
    EXPECT_EQ(receipts.size(), numRecords);
    
    // Bulk delete
    for (const auto& uuid : transactionUUIDs) {
        handler->deleteRecords(uuid);
    }
    
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time (10 seconds for 100 operations)
    EXPECT_LT(duration.count(), 10000);
}

// Error Handling Tests
TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, ErrorHandling_CorruptedDatabase_ThrowsIOError) {
    // Close the database to simulate corruption
    sqlite3_close(db);
    db = nullptr;
    
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    KeyHash::Shared keyHash = generateTestKeyHash();
    TrustLineAmount amount = generateTestAmount();
    
    // Operations should throw IOError
    EXPECT_THROW(
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount),
        IOError
    );
    
    EXPECT_THROW(
        handler->auditAmounts(trustLineID, auditNumber),
        IOError
    );
    
    EXPECT_THROW(
        handler->countReceiptsByNumber(trustLineID, auditNumber),
        IOError
    );
} 