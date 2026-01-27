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
#include "../../../src/core/io/storage/sqlite/PaymentTransactionsHandlerSQLite.h"
#include "../../../src/core/io/storage/interfaces/PaymentTransactionsHandler.h"
#include "../../../src/core/io/storage/sqlite/StorageHandlerSQLite.h"
#include "../../../src/core/common/exceptions/IOError.h"
#include "../../../src/core/common/exceptions/NotFoundError.h"
#include "../../../src/core/common/exceptions/ValueError.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/common/Types.h"

using namespace std;
using namespace testing;

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

        // Create payment_keys table required by payment transactions handler.
        const char* createKeysTable =
            "CREATE TABLE IF NOT EXISTS payment_keys ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "public_key BLOB NOT NULL, "
            "private_key BLOB NOT NULL);";
        char* errMsg = nullptr;
        int execRc = sqlite3_exec(db, createKeysTable, nullptr, nullptr, &errMsg);
        ASSERT_EQ(execRc, SQLITE_OK) << "Failed to create payment_keys table: " << (errMsg ? errMsg : "");
        if (errMsg) {
            sqlite3_free(errMsg);
            errMsg = nullptr;
        }

        const char* insertDummyKey =
            "INSERT INTO payment_keys (public_key, private_key) VALUES (zeroblob(32), zeroblob(64));";
        execRc = sqlite3_exec(db, insertDummyKey, nullptr, nullptr, &errMsg);
        ASSERT_EQ(execRc, SQLITE_OK) << "Failed to insert dummy key into payment_keys: " << (errMsg ? errMsg : "");
        if (errMsg) {
            sqlite3_free(errMsg);
            errMsg = nullptr;
        }

        paymentTransactionsHandler = make_unique<PaymentTransactionsHandlerSQLite>(
            db,
            StorageHandlerSQLite::paymentTransactionsTableName(),
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
    unique_ptr<PaymentTransactionsHandlerSQLite> paymentTransactionsHandler;
};

namespace {
const AuditNumber kTestZeroAuditNumber = 0;
const AuditNumber kTestUpdatedAuditNumber = 9;
const AuditNumber kTestDeleteAuditNumber = 5;
const AuditNumber kTestOtherAuditNumber = 6;
const BlockNumber kTestBlockNumber = 200;
const size_t kTestReceiptsToDelete = 2;
const size_t kTestReceiptsToKeep = 3;
} // namespace

static TransactionUUID makeUUIDWithPrefix(uint8_t prefix)
{
    uint8_t bytes[TransactionUUID::kBytesSize];
    memset(bytes, 0, sizeof(bytes));
    bytes[0] = prefix;
    return TransactionUUID(bytes);
}

static void savePaymentTransaction(
    PaymentTransactionsHandlerSQLite &handler,
    const TransactionUUID &transactionUUID,
    PaymentObservingState observingState)
{
    handler.saveRecord(transactionUUID, kTestBlockNumber, kTestBlockNumber);
    if (observingState != PaymentObservingState::Init) {
        handler.updateTransactionState(transactionUUID, observingState);
    }
}

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

// getFinalizedReceiptsWithZeroAuditNumber Tests
TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, GetFinalizedReceiptsWithZeroAuditNumber_FiltersByAuditNumberAndState) {
    TrustLineID trustLineID = generateTestTrustLineID();

    TransactionUUID committedUUID = makeUUIDWithPrefix(0xA1);
    TransactionUUID pendingUUID = makeUUIDWithPrefix(0xA2);
    TransactionUUID nonZeroAuditUUID = makeUUIDWithPrefix(0xA3);

    savePaymentTransaction(*paymentTransactionsHandler, committedUUID, PaymentObservingState::Committed);
    savePaymentTransaction(*paymentTransactionsHandler, pendingUUID, PaymentObservingState::Init);
    savePaymentTransaction(*paymentTransactionsHandler, nonZeroAuditUUID, PaymentObservingState::Committed);

    handler->saveRecord(trustLineID, kTestZeroAuditNumber, committedUUID, generateTestKeyHash(),
                        generateTestAmount());
    handler->saveRecord(trustLineID, kTestZeroAuditNumber, pendingUUID, generateTestKeyHash(),
                        generateTestAmount());
    handler->saveRecord(trustLineID, 2, nonZeroAuditUUID, generateTestKeyHash(),
                        generateTestAmount());

    auto results = handler->getFinalizedReceiptsWithZeroAuditNumber(trustLineID);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results.front()->transactionUUID(), committedUUID);
}

TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, GetFinalizedReceiptsWithZeroAuditNumber_NoMatches_ReturnsEmpty) {
    TrustLineID trustLineID = generateTestTrustLineID();

    auto results = handler->getFinalizedReceiptsWithZeroAuditNumber(trustLineID);
    EXPECT_TRUE(results.empty());
}

// updateAuditNumberByTransactionUUIDs Tests
TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, UpdateAuditNumberByTransactionUUIDs_UpdatesOnlySpecifiedReceipts) {
    TrustLineID trustLineID = generateTestTrustLineID();
    TrustLineID otherTrustLineID = generateTestTrustLineID();

    TransactionUUID updateUUID = makeUUIDWithPrefix(0xB1);
    TransactionUUID keepUUID = makeUUIDWithPrefix(0xB2);

    savePaymentTransaction(*paymentTransactionsHandler, updateUUID, PaymentObservingState::Committed);
    savePaymentTransaction(*paymentTransactionsHandler, keepUUID, PaymentObservingState::Committed);

    handler->saveRecord(trustLineID, kTestZeroAuditNumber, updateUUID, generateTestKeyHash(),
                        generateTestAmount());
    handler->saveRecord(trustLineID, kTestZeroAuditNumber, keepUUID, generateTestKeyHash(),
                        generateTestAmount());
    handler->saveRecord(otherTrustLineID, kTestZeroAuditNumber, updateUUID, generateTestKeyHash(),
                        generateTestAmount());

    handler->updateAuditNumberByTransactionUUIDs(
        trustLineID,
        kTestUpdatedAuditNumber,
        vector<TransactionUUID>{updateUUID});

    EXPECT_EQ(handler->countReceiptsByNumber(trustLineID, kTestUpdatedAuditNumber), 1);
    EXPECT_EQ(handler->countReceiptsByNumber(trustLineID, kTestZeroAuditNumber), 1);
    EXPECT_EQ(handler->countReceiptsByNumber(otherTrustLineID, kTestUpdatedAuditNumber), 0);
    EXPECT_EQ(handler->countReceiptsByNumber(otherTrustLineID, kTestZeroAuditNumber), 1);
}

TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, UpdateAuditNumberByTransactionUUIDs_EmptyList_DoesNotChangeReceipts) {
    TrustLineID trustLineID = generateTestTrustLineID();

    TransactionUUID transactionUUID = makeUUIDWithPrefix(0xC1);
    savePaymentTransaction(*paymentTransactionsHandler, transactionUUID, PaymentObservingState::Committed);

    handler->saveRecord(trustLineID, kTestZeroAuditNumber, transactionUUID, generateTestKeyHash(),
                        generateTestAmount());

    handler->updateAuditNumberByTransactionUUIDs(trustLineID, kTestUpdatedAuditNumber, {});

    EXPECT_EQ(handler->countReceiptsByNumber(trustLineID, kTestZeroAuditNumber), 1);
    EXPECT_EQ(handler->countReceiptsByNumber(trustLineID, kTestUpdatedAuditNumber), 0);
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

// deleteRecordsByAuditNumber Tests
TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, DeleteRecordsByAuditNumber_DeletesMatchingRecords) {
    TrustLineID trustLineID = generateTestTrustLineID();

    for (size_t i = 0; i < kTestReceiptsToDelete; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        handler->saveRecord(trustLineID, kTestDeleteAuditNumber, transactionUUID, generateTestKeyHash(),
                            generateTestAmount());
    }

    EXPECT_EQ(handler->countReceiptsByNumber(trustLineID, kTestDeleteAuditNumber), kTestReceiptsToDelete);

    EXPECT_NO_THROW(
        handler->deleteRecordsByAuditNumber(trustLineID, kTestDeleteAuditNumber)
    );

    EXPECT_TRUE(handler->receiptsByAuditNumber(trustLineID, kTestDeleteAuditNumber).empty());
}

TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, DeleteRecordsByAuditNumber_DoesNotDeleteOtherAuditNumbers) {
    TrustLineID trustLineID = generateTestTrustLineID();

    for (size_t i = 0; i < kTestReceiptsToDelete; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        handler->saveRecord(trustLineID, kTestDeleteAuditNumber, transactionUUID, generateTestKeyHash(),
                            generateTestAmount());
    }
    for (size_t i = 0; i < kTestReceiptsToKeep; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        handler->saveRecord(trustLineID, kTestOtherAuditNumber, transactionUUID, generateTestKeyHash(),
                            generateTestAmount());
    }

    EXPECT_NO_THROW(
        handler->deleteRecordsByAuditNumber(trustLineID, kTestDeleteAuditNumber)
    );

    EXPECT_TRUE(handler->receiptsByAuditNumber(trustLineID, kTestDeleteAuditNumber).empty());
    EXPECT_EQ(handler->receiptsByAuditNumber(trustLineID, kTestOtherAuditNumber).size(), kTestReceiptsToKeep);
}

TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, DeleteRecordsByAuditNumber_DoesNotDeleteOtherTrustLines) {
    TrustLineID trustLineID = generateTestTrustLineID();
    TrustLineID otherTrustLineID = generateTestTrustLineID();

    for (size_t i = 0; i < kTestReceiptsToDelete; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        handler->saveRecord(trustLineID, kTestDeleteAuditNumber, transactionUUID, generateTestKeyHash(),
                            generateTestAmount());
    }
    for (size_t i = 0; i < kTestReceiptsToKeep; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        handler->saveRecord(otherTrustLineID, kTestDeleteAuditNumber, transactionUUID, generateTestKeyHash(),
                            generateTestAmount());
    }

    EXPECT_NO_THROW(
        handler->deleteRecordsByAuditNumber(trustLineID, kTestDeleteAuditNumber)
    );

    EXPECT_TRUE(handler->receiptsByAuditNumber(trustLineID, kTestDeleteAuditNumber).empty());
    EXPECT_EQ(handler->receiptsByAuditNumber(otherTrustLineID, kTestDeleteAuditNumber).size(), kTestReceiptsToKeep);
}

TEST_F(OutgoingPaymentReceiptHandlerSQLiteTest, DeleteRecordsByAuditNumber_HandlesEmptyTable) {
    TrustLineID trustLineID = generateTestTrustLineID();

    EXPECT_NO_THROW(
        handler->deleteRecordsByAuditNumber(trustLineID, kTestDeleteAuditNumber)
    );

    EXPECT_EQ(handler->countReceiptsByNumber(trustLineID, kTestDeleteAuditNumber), 0);
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