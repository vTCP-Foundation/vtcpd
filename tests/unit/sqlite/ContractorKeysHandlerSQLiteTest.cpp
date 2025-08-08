#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sqlite3.h>
#include <memory>
#include <string>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <chrono>

#include "../../../src/core/io/storage/sqlite/ContractorKeysHandlerSQLite.h"
#include "../../../src/core/common/exceptions/IOError.h"
#include "../../../src/core/common/exceptions/NotFoundError.h"
#include "../../../src/core/common/exceptions/ValueError.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/common/Types.h"

using namespace std;
using namespace testing;

// -----------------------------------------------------------------------------
// Minimal stub factory to generate TrustLineID values (replaces obsolete TestDataFactory).
class DummyFactory {
public:
    TrustLineID generateTrustLineID() {
        static TrustLineID sID = 1;
        return sID++;
    }
};

class ContractorKeysHandlerSQLiteTest : public Test {
protected:
    void SetUp() override {
        // Create temporary directory for test database
        tempDir = filesystem::temp_directory_path() / "contractor_keys_test";
        filesystem::create_directories(tempDir);
        
        // Create test database
        testDbPath = tempDir / "test.db";
        int rc = sqlite3_open(testDbPath.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK);
        
        // Create Logger
        logger = make_unique<Logger>();
        
        // Create dummy data factory
        testDataFactory = make_unique<DummyFactory>();
        
        // Create handler
        handler = make_unique<ContractorKeysHandlerSQLite>(
            db, 
            "contractor_keys_test", 
            *logger
        );
    }
    
    void TearDown() override {
        handler.reset();
        logger.reset();
        testDataFactory.reset();
        
        if (db) {
            sqlite3_close(db);
        }
        
        filesystem::remove_all(tempDir);
    }
    
    // Helper methods
    PublicKey::Shared generateTestPublicKey() {
        constexpr size_t kSize = PublicKey::keySize();
        static uint8_t counter = 0;
        byte_t keyData[kSize];
        std::fill(keyData, keyData + kSize, counter++);
        return make_shared<PublicKey>(keyData);
    }
    
    KeyHash::Shared generateTestKeyHash() {
        byte_t hashData[KeyHash::kBytesSize];
        fill(hashData, hashData + KeyHash::kBytesSize, 0xEF);
        return make_shared<KeyHash>(hashData);
    }
    
    filesystem::path tempDir;
    filesystem::path testDbPath;
    sqlite3* db = nullptr;
    unique_ptr<Logger> logger;
    unique_ptr<DummyFactory> testDataFactory;
    unique_ptr<ContractorKeysHandlerSQLite> handler;
};

// Constructor Tests
TEST_F(ContractorKeysHandlerSQLiteTest, Constructor_ValidParameters_CreatesTableAndIndexes) {
    // Verify table exists
    string query = "SELECT name FROM sqlite_master WHERE type='table' AND name='contractor_keys_test';";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt);
    EXPECT_EQ(rc, SQLITE_ROW);
    
    string tableName = (char*)sqlite3_column_text(stmt, 0);
    EXPECT_EQ(tableName, "contractor_keys_test");
    
    sqlite3_finalize(stmt);
    
    // Verify indexes exist
    query = "SELECT name FROM sqlite_master WHERE type='index' AND name LIKE 'contractor_keys_test%';";
    rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    int indexCount = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        indexCount++;
    }
    EXPECT_GT(indexCount, 0);  // Should have at least one index
    
    sqlite3_finalize(stmt);
}

TEST_F(ContractorKeysHandlerSQLiteTest, Constructor_NullDatabase_ThrowsException) {
    EXPECT_THROW(
        ContractorKeysHandlerSQLite(nullptr, "test_table", *logger),
        ValueError
    );
}

TEST_F(ContractorKeysHandlerSQLiteTest, Constructor_EmptyTableName_ThrowsException) {
    EXPECT_THROW(
        ContractorKeysHandlerSQLite(db, "", *logger),
        ValueError
    );
}

// saveKey Tests
TEST_F(ContractorKeysHandlerSQLiteTest, SaveKey_ValidParameters_SavesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    KeyNumber keyNumber = 1;
    
    EXPECT_NO_THROW(
        handler->saveKey(trustLineID, sequenceNumber, publicKey, keyNumber)
    );
    
    // Verify key was saved
    KeysCount count = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(count, 1);
}

TEST_F(ContractorKeysHandlerSQLiteTest, SaveKey_NullPublicKey_ThrowsException) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    KeyNumber keyNumber = 1;
    
    EXPECT_THROW(
        handler->saveKey(trustLineID, sequenceNumber, nullptr, keyNumber),
        ValueError
    );
}

TEST_F(ContractorKeysHandlerSQLiteTest, SaveKey_MultipleKeys_SavesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    const int numKeys = 5;
    
    for (int i = 0; i < numKeys; ++i) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        KeyNumber keyNumber = i + 1;
        
        EXPECT_NO_THROW(
            handler->saveKey(trustLineID, sequenceNumber, publicKey, keyNumber)
        );
    }
    
    // Verify all keys were saved
    KeysCount count = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(count, numKeys);
}

// maxKeySetSequenceNumber Tests
TEST_F(ContractorKeysHandlerSQLiteTest, MaxKeySetSequenceNumber_WithKeys_ReturnsCorrectValue) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    // Save keys with different sequence numbers
    vector<KeyNumber> sequenceNumbers = {1, 3, 5, 7, 9};
    for (KeyNumber seqNum : sequenceNumbers) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        handler->saveKey(trustLineID, seqNum, publicKey, 1);
    }
    
    KeyNumber maxSeqNum = handler->maxKeySetSequenceNumber(trustLineID);
    EXPECT_EQ(maxSeqNum, 9);
}

TEST_F(ContractorKeysHandlerSQLiteTest, MaxKeySetSequenceNumber_NoKeys_ThrowsNotFoundError) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_THROW(
        handler->maxKeySetSequenceNumber(trustLineID),
        NotFoundError
    );
}

// invalidKey Tests
TEST_F(ContractorKeysHandlerSQLiteTest, InvalidKey_ValidParameters_InvalidatesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    KeyNumber keyNumber = 1;
    
    // Save a key
    handler->saveKey(trustLineID, sequenceNumber, publicKey, keyNumber);
    
    // Verify key is available
    KeysCount countBefore = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(countBefore, 1);
    
    // Invalidate the key
    EXPECT_NO_THROW(
        handler->invalidKey(trustLineID, keyNumber)
    );
    
    // Verify key is no longer available
    KeysCount countAfter = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(countAfter, 0);
}

TEST_F(ContractorKeysHandlerSQLiteTest, InvalidKey_NonExistentKey_ThrowsException) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber keyNumber = 999;
    
    EXPECT_THROW(
        handler->invalidKey(trustLineID, keyNumber),
        ValueError
    );
}

// invalidateKeyByHash Tests
TEST_F(ContractorKeysHandlerSQLiteTest, InvalidateKeyByHash_ValidParameters_InvalidatesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    KeyNumber keyNumber = 1;
    
    // Save a key
    handler->saveKey(trustLineID, sequenceNumber, publicKey, keyNumber);
    
    // Get key hash
    KeyHash::Shared keyHash = handler->keyHashByNumber(trustLineID, keyNumber);
    
    // Verify key is available
    KeysCount countBefore = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(countBefore, 1);
    
    // Invalidate the key by hash
    EXPECT_NO_THROW(
        handler->invalidateKeyByHash(trustLineID, keyHash)
    );
    
    // Verify key is no longer available
    KeysCount countAfter = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(countAfter, 0);
}

TEST_F(ContractorKeysHandlerSQLiteTest, InvalidateKeyByHash_NullKeyHash_ThrowsException) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_THROW(
        handler->invalidateKeyByHash(trustLineID, nullptr),
        ValueError
    );
}

// keyByNumber Tests
TEST_F(ContractorKeysHandlerSQLiteTest, KeyByNumber_ExistingKey_ReturnsKey) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    KeyNumber keyNumber = 1;
    
    // Save a key
    handler->saveKey(trustLineID, sequenceNumber, publicKey, keyNumber);
    
    // Retrieve the key
    PublicKey::Shared retrievedKey = handler->keyByNumber(trustLineID, keyNumber);
    EXPECT_NE(retrievedKey, nullptr);
}

TEST_F(ContractorKeysHandlerSQLiteTest, KeyByNumber_NonExistentKey_ThrowsNotFoundError) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber keyNumber = 999;
    
    EXPECT_THROW(
        handler->keyByNumber(trustLineID, keyNumber),
        NotFoundError
    );
}

// keyByHash Tests
TEST_F(ContractorKeysHandlerSQLiteTest, KeyByHash_ExistingKey_ReturnsKey) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    KeyNumber keyNumber = 1;
    
    // Save a key
    handler->saveKey(trustLineID, sequenceNumber, publicKey, keyNumber);
    
    // Get key hash
    KeyHash::Shared keyHash = handler->keyHashByNumber(trustLineID, keyNumber);
    
    // Retrieve the key by hash
    PublicKey::Shared retrievedKey = handler->keyByHash(trustLineID, keyHash);
    EXPECT_NE(retrievedKey, nullptr);
}

TEST_F(ContractorKeysHandlerSQLiteTest, KeyByHash_NullKeyHash_ThrowsException) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_THROW(
        handler->keyByHash(trustLineID, nullptr),
        ValueError
    );
}

// keyHashByNumber Tests
TEST_F(ContractorKeysHandlerSQLiteTest, KeyHashByNumber_ExistingKey_ReturnsHash) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    KeyNumber keyNumber = 1;
    
    // Save a key
    handler->saveKey(trustLineID, sequenceNumber, publicKey, keyNumber);
    
    // Retrieve the key hash
    KeyHash::Shared keyHash = handler->keyHashByNumber(trustLineID, keyNumber);
    EXPECT_NE(keyHash, nullptr);
}

TEST_F(ContractorKeysHandlerSQLiteTest, KeyHashByNumber_NonExistentKey_ThrowsNotFoundError) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber keyNumber = 999;
    
    EXPECT_THROW(
        handler->keyHashByNumber(trustLineID, keyNumber),
        NotFoundError
    );
}

// availableKeysCnt Tests
TEST_F(ContractorKeysHandlerSQLiteTest, AvailableKeysCnt_WithKeys_ReturnsCorrectCount) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    const int numKeys = 10;
    
    // Save multiple keys
    for (int i = 0; i < numKeys; ++i) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        KeyNumber keyNumber = i + 1;
        
        handler->saveKey(trustLineID, sequenceNumber, publicKey, keyNumber);
    }
    
    KeysCount count = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(count, numKeys);
}

TEST_F(ContractorKeysHandlerSQLiteTest, AvailableKeysCnt_NoKeys_ReturnsZero) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    KeysCount count = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(count, 0);
}

// sequenceKeysCnt Tests
TEST_F(ContractorKeysHandlerSQLiteTest, SequenceKeysCnt_WithKeys_ReturnsCorrectCount) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    const int numKeys = 5;
    
    // Save multiple keys in same sequence
    for (int i = 0; i < numKeys; ++i) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        KeyNumber keyNumber = i + 1;
        
        handler->saveKey(trustLineID, sequenceNumber, publicKey, keyNumber);
    }
    
    KeysCount count = handler->sequenceKeysCnt(trustLineID, sequenceNumber);
    EXPECT_EQ(count, numKeys);
}

TEST_F(ContractorKeysHandlerSQLiteTest, SequenceKeysCnt_NoKeys_ReturnsZero) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    
    KeysCount count = handler->sequenceKeysCnt(trustLineID, sequenceNumber);
    EXPECT_EQ(count, 0);
}

// removeUnusedKeys Tests
TEST_F(ContractorKeysHandlerSQLiteTest, RemoveUnusedKeys_WithKeys_RemovesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    const int numKeys = 5;
    
    // Save multiple keys
    for (int i = 0; i < numKeys; ++i) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        KeyNumber keyNumber = i + 1;
        
        handler->saveKey(trustLineID, sequenceNumber, publicKey, keyNumber);
    }
    
    // Verify keys exist
    KeysCount countBefore = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(countBefore, numKeys);
    
    // Remove unused keys
    EXPECT_NO_THROW(
        handler->removeUnusedKeys(trustLineID)
    );
    
    // Verify keys are removed
    KeysCount countAfter = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(countAfter, 0);
}

TEST_F(ContractorKeysHandlerSQLiteTest, RemoveUnusedKeys_NoKeys_DoesNotThrow) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_NO_THROW(
        handler->removeUnusedKeys(trustLineID)
    );
}

// deleteKeysByTrustLineID Tests
TEST_F(ContractorKeysHandlerSQLiteTest, DeleteKeysByTrustLineID_WithKeys_DeletesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    const int numKeys = 5;
    
    // Save multiple keys
    for (int i = 0; i < numKeys; ++i) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        KeyNumber keyNumber = i + 1;
        
        handler->saveKey(trustLineID, sequenceNumber, publicKey, keyNumber);
    }
    
    // Verify keys exist
    KeysCount countBefore = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(countBefore, numKeys);
    
    // Delete all keys
    EXPECT_NO_THROW(
        handler->deleteKeysByTrustLineID(trustLineID)
    );
    
    // Verify keys are deleted
    KeysCount countAfter = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(countAfter, 0);
}

TEST_F(ContractorKeysHandlerSQLiteTest, DeleteKeysByTrustLineID_NoKeys_DoesNotThrow) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_NO_THROW(
        handler->deleteKeysByTrustLineID(trustLineID)
    );
}

// Integration Tests
TEST_F(ContractorKeysHandlerSQLiteTest, Integration_SaveInvalidateDelete_WorksCorrectly) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    KeyNumber keyNumber = 1;
    
    // Save key
    EXPECT_NO_THROW(
        handler->saveKey(trustLineID, sequenceNumber, publicKey, keyNumber)
    );
    
    // Verify key is available
    KeysCount count = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(count, 1);
    
    // Invalidate key
    EXPECT_NO_THROW(
        handler->invalidKey(trustLineID, keyNumber)
    );
    
    // Verify key is no longer available
    count = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(count, 0);
    
    // Delete all keys
    EXPECT_NO_THROW(
        handler->deleteKeysByTrustLineID(trustLineID)
    );
}

// Performance Tests
TEST_F(ContractorKeysHandlerSQLiteTest, Performance_BulkOperations_CompletesInReasonableTime) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    const int numKeys = 100;
    
    auto start = chrono::high_resolution_clock::now();
    
    // Bulk save
    for (int i = 0; i < numKeys; ++i) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        KeyNumber keyNumber = i + 1;
        
        handler->saveKey(trustLineID, sequenceNumber, publicKey, keyNumber);
    }
    
    // Bulk count
    KeysCount count = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(count, numKeys);
    
    // Bulk delete
    handler->deleteKeysByTrustLineID(trustLineID);
    
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time (5 seconds for 100 operations)
    EXPECT_LT(duration.count(), 5000);
}

// Error Handling Tests
TEST_F(ContractorKeysHandlerSQLiteTest, ErrorHandling_CorruptedDatabase_ThrowsIOError) {
    // Close the database to simulate corruption
    sqlite3_close(db);
    db = nullptr;
    
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    KeyNumber keyNumber = 1;
    
    // Operations should throw IOError
    EXPECT_THROW(
        handler->saveKey(trustLineID, sequenceNumber, publicKey, keyNumber),
        IOError
    );
    
    EXPECT_THROW(
        handler->availableKeysCnt(trustLineID),
        IOError
    );
} 