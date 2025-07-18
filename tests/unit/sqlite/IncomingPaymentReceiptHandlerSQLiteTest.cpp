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

#include "../../../src/core/io/storage/sqlite/IncomingPaymentReceiptHandlerSQLite.h"
#include "../../../src/core/common/exceptions/IOError.h"
#include "../../../src/core/common/exceptions/NotFoundError.h"
#include "../../../src/core/common/exceptions/ValueError.h"
#include "../../../src/core/crypto/lamportkeys.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/common/Types.h"

using namespace std;
using namespace testing;
using namespace crypto::lamport;

class IncomingPaymentReceiptHandlerSQLiteTest : public Test {
protected:
    void SetUp() override {
        // Create temporary directory for test database
        tempDir = filesystem::temp_directory_path() / "incoming_receipt_test";
        filesystem::create_directories(tempDir);
        
        // Create test database
        testDbPath = tempDir / "test.db";
        int rc = sqlite3_open(testDbPath.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK);
        
        // Create Logger
        logger = make_unique<Logger>();
        
        // Create handler
        handler = make_unique<IncomingPaymentReceiptHandlerSQLite>(
            db, 
            "incoming_receipts_test", 
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
    
    Signature::Shared generateTestSignature() {
        byte_t signatureData[Signature::kSize];
        fill(signatureData, signatureData + Signature::kSize, 0xCD);
        return make_shared<Signature>(signatureData);
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
    unique_ptr<IncomingPaymentReceiptHandlerSQLite> handler;
};

// Constructor Tests
TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, Constructor_ValidParameters_CreatesTableAndIndexes) {
    // Verify table exists
    string query = "SELECT name FROM sqlite_master WHERE type='table' AND name='incoming_receipts_test';";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt);
    EXPECT_EQ(rc, SQLITE_ROW);
    
    string tableName = (char*)sqlite3_column_text(stmt, 0);
    EXPECT_EQ(tableName, "incoming_receipts_test");
    
    sqlite3_finalize(stmt);
    
    // Verify indexes exist
    query = "SELECT name FROM sqlite_master WHERE type='index' AND name LIKE 'incoming_receipts_test%';";
    rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    int indexCount = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        indexCount++;
    }
    EXPECT_GT(indexCount, 0);  // Should have at least one index
    
    sqlite3_finalize(stmt);
}

TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, Constructor_NullDatabase_ThrowsException) {
    EXPECT_THROW(
        IncomingPaymentReceiptHandlerSQLite(nullptr, "test_table", *logger),
        std::exception
    );
}

TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, Constructor_EmptyTableName_ThrowsException) {
    EXPECT_THROW(
        IncomingPaymentReceiptHandlerSQLite(db, "", *logger),
        std::exception
    );
}

// saveRecord Tests
TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, SaveRecord_ValidParameters_SavesSuccessfully) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    KeyHash::Shared keyHash = generateTestKeyHash();
    TrustLineAmount amount = generateTestAmount();
    Signature::Shared signature = generateTestSignature();
    
    EXPECT_NO_THROW(
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount, signature)
    );
    
    // Verify record was saved
    bool containsTransaction = handler->isContainsTransaction(transactionUUID);
    EXPECT_TRUE(containsTransaction);
    
    bool containsKeyHash = handler->isContainsKeyHash(keyHash);
    EXPECT_TRUE(containsKeyHash);
}

TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, SaveRecord_NullKeyHash_ThrowsException) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    TrustLineAmount amount = generateTestAmount();
    Signature::Shared signature = generateTestSignature();
    
    EXPECT_THROW(
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, nullptr, amount, signature),
        ValueError
    );
}

TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, SaveRecord_NullSignature_ThrowsException) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    KeyHash::Shared keyHash = generateTestKeyHash();
    TrustLineAmount amount = generateTestAmount();
    
    EXPECT_THROW(
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount, nullptr),
        ValueError
    );
}

TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, SaveRecord_MultipleRecords_SavesSuccessfully) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    const int numRecords = 5;
    
    vector<TransactionUUID> transactionUUIDs;
    vector<KeyHash::Shared> keyHashes;
    
    for (int i = 0; i < numRecords; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        KeyHash::Shared keyHash = generateTestKeyHash();
        TrustLineAmount amount = generateTestAmount();
        Signature::Shared signature = generateTestSignature();
        
        transactionUUIDs.push_back(transactionUUID);
        keyHashes.push_back(keyHash);
        
        EXPECT_NO_THROW(
            handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount, signature)
        );
    }
    
    // Verify all records were saved
    size_t count = handler->countReceiptsByNumber(trustLineID, auditNumber);
    EXPECT_EQ(count, numRecords);
    
    for (const auto& uuid : transactionUUIDs) {
        EXPECT_TRUE(handler->isContainsTransaction(uuid));
    }
    
    for (const auto& hash : keyHashes) {
        EXPECT_TRUE(handler->isContainsKeyHash(hash));
    }
}

// auditAmounts Tests
TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, AuditAmounts_WithRecords_ReturnsCorrectAmounts) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    
    map<TransactionUUID, TrustLineAmount> expectedAmounts;
    
    for (int i = 0; i < 3; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        KeyHash::Shared keyHash = generateTestKeyHash();
        TrustLineAmount amount = TrustLineAmount(i + 1);
        Signature::Shared signature = generateTestSignature();
        
        expectedAmounts[transactionUUID] = amount;
        
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount, signature);
    }
    
    map<TransactionUUID, TrustLineAmount> retrievedAmounts = handler->auditAmounts(trustLineID, auditNumber);
    
    EXPECT_EQ(retrievedAmounts.size(), expectedAmounts.size());
    for (const auto& pair : expectedAmounts) {
        EXPECT_EQ(retrievedAmounts[pair.first], pair.second);
    }
}

TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, AuditAmounts_NoRecords_ReturnsEmptyMap) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    
    map<TransactionUUID, TrustLineAmount> amounts = handler->auditAmounts(trustLineID, auditNumber);
    EXPECT_TRUE(amounts.empty());
}

// receiptsByAuditNumber Tests
TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, ReceiptsByAuditNumber_WithRecords_ReturnsCorrectReceipts) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    const int numRecords = 3;
    
    // Save records
    for (int i = 0; i < numRecords; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        KeyHash::Shared keyHash = generateTestKeyHash();
        TrustLineAmount amount = generateTestAmount();
        Signature::Shared signature = generateTestSignature();
        
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount, signature);
    }
    
    vector<ReceiptRecord::Shared> receipts = handler->receiptsByAuditNumber(trustLineID, auditNumber);
    EXPECT_EQ(receipts.size(), numRecords);
}

TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, ReceiptsByAuditNumber_NoRecords_ReturnsEmptyVector) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    
    vector<ReceiptRecord::Shared> receipts = handler->receiptsByAuditNumber(trustLineID, auditNumber);
    EXPECT_TRUE(receipts.empty());
}

// receiptsLessEqualThanAuditNumber Tests
TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, ReceiptsLessEqualThanAuditNumber_WithRecords_ReturnsCorrectReceipts) {
    TrustLineID trustLineID = generateTestTrustLineID();
    
    // Save records with different audit numbers
    for (int auditNum = 1; auditNum <= 5; ++auditNum) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        KeyHash::Shared keyHash = generateTestKeyHash();
        TrustLineAmount amount = generateTestAmount();
        Signature::Shared signature = generateTestSignature();
        
        handler->saveRecord(trustLineID, auditNum, transactionUUID, keyHash, amount, signature);
    }
    
    // Get receipts with audit number <= 3
    vector<ReceiptRecord::Shared> receipts = handler->receiptsLessEqualThanAuditNumber(trustLineID, 3);
    EXPECT_EQ(receipts.size(), 3);
}

TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, ReceiptsLessEqualThanAuditNumber_NoRecords_ReturnsEmptyVector) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    
    vector<ReceiptRecord::Shared> receipts = handler->receiptsLessEqualThanAuditNumber(trustLineID, auditNumber);
    EXPECT_TRUE(receipts.empty());
}

// deleteRecords Tests
TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, DeleteRecords_ByTransactionUUID_DeletesSuccessfully) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    KeyHash::Shared keyHash = generateTestKeyHash();
    TrustLineAmount amount = generateTestAmount();
    Signature::Shared signature = generateTestSignature();
    
    // Save record
    handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount, signature);
    
    // Verify record exists
    EXPECT_TRUE(handler->isContainsTransaction(transactionUUID));
    
    // Delete record
    EXPECT_NO_THROW(
        handler->deleteRecords(transactionUUID)
    );
    
    // Verify record is deleted
    EXPECT_FALSE(handler->isContainsTransaction(transactionUUID));
}

TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, DeleteRecords_ByTrustLineID_DeletesSuccessfully) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    const int numRecords = 3;
    
    vector<TransactionUUID> transactionUUIDs;
    
    // Save multiple records
    for (int i = 0; i < numRecords; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        KeyHash::Shared keyHash = generateTestKeyHash();
        TrustLineAmount amount = generateTestAmount();
        Signature::Shared signature = generateTestSignature();
        
        transactionUUIDs.push_back(transactionUUID);
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount, signature);
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

TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, DeleteRecords_NonExistentTransaction_DoesNotThrow) {
    TrustLineID trustLineID = generateTestTrustLineID();
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    
    EXPECT_NO_THROW(
        handler->deleteRecords(transactionUUID)
    );
}

// isContainsKeyHash Tests
TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, IsContainsKeyHash_ExistingKeyHash_ReturnsTrue) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    KeyHash::Shared keyHash = generateTestKeyHash();
    TrustLineAmount amount = generateTestAmount();
    Signature::Shared signature = generateTestSignature();
    
    // Save record
    handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount, signature);
    
    // Check if key hash exists
    bool contains = handler->isContainsKeyHash(keyHash);
    EXPECT_TRUE(contains);
}

TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, IsContainsKeyHash_NonExistentKeyHash_ReturnsFalse) {
    TrustLineID trustLineID = generateTestTrustLineID();
    KeyHash::Shared keyHash = generateTestKeyHash();
    
    bool contains = handler->isContainsKeyHash(keyHash);
    EXPECT_FALSE(contains);
}

TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, IsContainsKeyHash_NullKeyHash_ThrowsException) {
    TrustLineID trustLineID = generateTestTrustLineID();
    EXPECT_THROW(
        handler->isContainsKeyHash(nullptr),
        ValueError
    );
}

// isContainsTransaction Tests
TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, IsContainsTransaction_ExistingTransaction_ReturnsTrue) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    KeyHash::Shared keyHash = generateTestKeyHash();
    TrustLineAmount amount = generateTestAmount();
    Signature::Shared signature = generateTestSignature();
    
    // Save record
    handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount, signature);
    
    // Check if transaction exists
    bool contains = handler->isContainsTransaction(transactionUUID);
    EXPECT_TRUE(contains);
}

TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, IsContainsTransaction_NonExistentTransaction_ReturnsFalse) {
    TrustLineID trustLineID = generateTestTrustLineID();
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    
    bool contains = handler->isContainsTransaction(transactionUUID);
    EXPECT_FALSE(contains);
}

// countReceiptsByNumber Tests
TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, CountReceiptsByNumber_WithRecords_ReturnsCorrectCount) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    const int numRecords = 5;
    
    // Save multiple records
    for (int i = 0; i < numRecords; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        KeyHash::Shared keyHash = generateTestKeyHash();
        TrustLineAmount amount = generateTestAmount();
        Signature::Shared signature = generateTestSignature();
        
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount, signature);
    }
    
    size_t count = handler->countReceiptsByNumber(trustLineID, auditNumber);
    EXPECT_EQ(count, numRecords);
}

TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, CountReceiptsByNumber_NoRecords_ReturnsZero) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    
    size_t count = handler->countReceiptsByNumber(trustLineID, auditNumber);
    EXPECT_EQ(count, 0);
}

// Integration Tests
TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, Integration_SaveRetrieveDelete_WorksCorrectly) {
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    KeyHash::Shared keyHash = generateTestKeyHash();
    TrustLineAmount amount = TrustLineAmount(100);
    Signature::Shared signature = generateTestSignature();
    
    // Save record
    EXPECT_NO_THROW(
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount, signature)
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
    
    count = handler->countReceiptsByNumber(trustLineID, auditNumber);
    EXPECT_EQ(count, 0);
}

// Performance Tests
TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, Performance_BulkOperations_CompletesInReasonableTime) {
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
        Signature::Shared signature = generateTestSignature();
        
        transactionUUIDs.push_back(transactionUUID);
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount, signature);
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
TEST_F(IncomingPaymentReceiptHandlerSQLiteTest, ErrorHandling_CorruptedDatabase_ThrowsIOError) {
    // Close the database to simulate corruption
    sqlite3_close(db);
    db = nullptr;
    
    TrustLineID trustLineID = generateTestTrustLineID();
    AuditNumber auditNumber = 1;
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    KeyHash::Shared keyHash = generateTestKeyHash();
    TrustLineAmount amount = generateTestAmount();
    Signature::Shared signature = generateTestSignature();
    
    // Operations should throw IOError
    EXPECT_THROW(
        handler->saveRecord(trustLineID, auditNumber, transactionUUID, keyHash, amount, signature),
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