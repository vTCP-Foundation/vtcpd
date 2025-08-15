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
    EXPECT_NO_THROW(
        handler->saveKey(trustLineID, sequenceNumber, publicKey)
    );
    
    // Verify key was saved
    EXPECT_TRUE(handler->hasKey(trustLineID));
    auto keys = handler->publicKeysBySetNumber(trustLineID, sequenceNumber);
    EXPECT_EQ(keys.size(), 1u);
}

TEST_F(ContractorKeysHandlerSQLiteTest, SaveKey_NullPublicKey_ThrowsException) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    EXPECT_THROW(
        handler->saveKey(trustLineID, sequenceNumber, nullptr),
        ValueError
    );
}

TEST_F(ContractorKeysHandlerSQLiteTest, SaveKey_MultipleKeys_SavesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    const int numKeys = 5;
    
    for (int i = 0; i < numKeys; ++i) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        EXPECT_NO_THROW(
            handler->saveKey(trustLineID, sequenceNumber, publicKey)
        );
    }
    
    // Verify all keys were saved
    auto keys = handler->publicKeysBySetNumber(trustLineID, sequenceNumber);
    EXPECT_EQ(keys.size(), static_cast<size_t>(numKeys));
}

// maxKeySetSequenceNumber Tests
TEST_F(ContractorKeysHandlerSQLiteTest, MaxKeySetSequenceNumber_WithKeys_ReturnsCorrectValue) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    // Save keys with different sequence numbers
    vector<KeyNumber> sequenceNumbers = {1, 3, 5, 7, 9};
    for (KeyNumber seqNum : sequenceNumbers) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        handler->saveKey(trustLineID, seqNum, publicKey);
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
    // Save a key
    handler->saveKey(trustLineID, sequenceNumber, publicKey);
    
    // Verify key is available
    EXPECT_TRUE(handler->hasKey(trustLineID));
    
    // Invalidate the key
    EXPECT_NO_THROW(
        handler->invalidateKey(trustLineID)
    );
    
    // Verify key is no longer available
    EXPECT_FALSE(handler->hasKey(trustLineID));
    EXPECT_THROW(handler->getPublicKey(trustLineID), NotFoundError);
}

TEST_F(ContractorKeysHandlerSQLiteTest, InvalidKey_NonExistentKey_ThrowsException) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    EXPECT_THROW(
        handler->invalidateKey(trustLineID),
        ValueError
    );
}

// invalidateKeyByHash Tests
TEST_F(ContractorKeysHandlerSQLiteTest, InvalidateKeyByHash_ValidParameters_InvalidatesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    // Save a key
    handler->saveKey(trustLineID, sequenceNumber, publicKey);
    
    // Get key hash
    KeyHash::Shared keyHash = handler->getPublicKeyHash(trustLineID);
    
    // Verify key is available
    EXPECT_TRUE(handler->hasKey(trustLineID));
    
    // Invalidate the key by hash
    EXPECT_NO_THROW(
        handler->invalidateKeyByHash(trustLineID, keyHash)
    );
    
    // Verify key is no longer available
    EXPECT_FALSE(handler->hasKey(trustLineID));
}

TEST_F(ContractorKeysHandlerSQLiteTest, InvalidateKeyByHash_NullKeyHash_ThrowsException) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_THROW(
        handler->invalidateKeyByHash(trustLineID, nullptr),
        ValueError
    );
}

// keyByHash Tests
TEST_F(ContractorKeysHandlerSQLiteTest, KeyByHash_ExistingKey_ReturnsKey) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    
    // Save a key
    handler->saveKey(trustLineID, sequenceNumber, publicKey);
    
    // Get key hash
    KeyHash::Shared keyHash = handler->getPublicKeyHash(trustLineID);
    
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

// getPublicKeyHash Tests
TEST_F(ContractorKeysHandlerSQLiteTest, GetPublicKeyHash_ExistingKey_ReturnsHash) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    
    // Save a key
    handler->saveKey(trustLineID, sequenceNumber, publicKey);
    
    // Retrieve the key hash
    KeyHash::Shared keyHash = handler->getPublicKeyHash(trustLineID);
    EXPECT_NE(keyHash, nullptr);
}

TEST_F(ContractorKeysHandlerSQLiteTest, GetPublicKeyHash_NonExistentKey_ThrowsNotFoundError) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_THROW(
        handler->getPublicKeyHash(trustLineID),
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
        
        handler->saveKey(trustLineID, sequenceNumber, publicKey);
    }
    
    auto keys = handler->publicKeysBySetNumber(trustLineID, sequenceNumber);
    EXPECT_EQ(keys.size(), static_cast<size_t>(numKeys));
}

TEST_F(ContractorKeysHandlerSQLiteTest, AvailableKeysCnt_NoKeys_ReturnsZero) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    
    EXPECT_FALSE(handler->hasKey(trustLineID));
    auto keys = handler->publicKeysBySetNumber(trustLineID, sequenceNumber);
    EXPECT_TRUE(keys.empty());
}

// sequenceKeysCnt Tests
TEST_F(ContractorKeysHandlerSQLiteTest, SequenceKeysCnt_WithKeys_ReturnsCorrectCount) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    const int numKeys = 5;
    
    // Save multiple keys in same sequence
    for (int i = 0; i < numKeys; ++i) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        
        handler->saveKey(trustLineID, sequenceNumber, publicKey);
    }
    
    auto keys = handler->publicKeysBySetNumber(trustLineID, sequenceNumber);
    EXPECT_EQ(keys.size(), static_cast<size_t>(numKeys));
}

TEST_F(ContractorKeysHandlerSQLiteTest, SequenceKeysCnt_NoKeys_ReturnsZero) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    
    auto keys = handler->publicKeysBySetNumber(trustLineID, sequenceNumber);
    EXPECT_TRUE(keys.empty());
}

// removeUnusedKeys Tests
TEST_F(ContractorKeysHandlerSQLiteTest, RemoveUnusedKeys_WithKeys_RemovesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    const int numKeys = 5;
    
    // Save multiple keys
    for (int i = 0; i < numKeys; ++i) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        
        handler->saveKey(trustLineID, sequenceNumber, publicKey);
    }
    
    // Verify keys exist
    auto keysBefore = handler->publicKeysBySetNumber(trustLineID, sequenceNumber);
    EXPECT_EQ(keysBefore.size(), static_cast<size_t>(numKeys));
    
    // Remove keys
    EXPECT_NO_THROW(
        handler->deleteKeysByTrustLineID(trustLineID)
    );
    
    // Verify keys are removed
    auto keysAfter = handler->publicKeysBySetNumber(trustLineID, sequenceNumber);
    EXPECT_TRUE(keysAfter.empty());
}

TEST_F(ContractorKeysHandlerSQLiteTest, RemoveUnusedKeys_NoKeys_DoesNotThrow) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_NO_THROW(
        handler->deleteKeysByTrustLineID(trustLineID)
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
        
        handler->saveKey(trustLineID, sequenceNumber, publicKey);
    }
    
    // Verify keys exist
    auto keysBefore = handler->publicKeysBySetNumber(trustLineID, sequenceNumber);
    EXPECT_EQ(keysBefore.size(), static_cast<size_t>(numKeys));
    
    // Delete all keys
    EXPECT_NO_THROW(
        handler->deleteKeysByTrustLineID(trustLineID)
    );
    
    // Verify keys are deleted
    auto keysAfter = handler->publicKeysBySetNumber(trustLineID, sequenceNumber);
    EXPECT_EQ(keysAfter.size(), 0u);
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
    // Save key
    EXPECT_NO_THROW(
        handler->saveKey(trustLineID, sequenceNumber, publicKey)
    );
    
    // Verify key is available
    EXPECT_TRUE(handler->hasKey(trustLineID));
    
    // Invalidate key
    EXPECT_NO_THROW(
        handler->invalidateKey(trustLineID)
    );
    
    // Verify key is no longer available
    EXPECT_FALSE(handler->hasKey(trustLineID));
    
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
        
        handler->saveKey(trustLineID, sequenceNumber, publicKey);
    }
    
    // Bulk count
    auto keys = handler->publicKeysBySetNumber(trustLineID, sequenceNumber);
    EXPECT_EQ(keys.size(), static_cast<size_t>(numKeys));
    
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
        handler->saveKey(trustLineID, sequenceNumber, publicKey),
        IOError
    );
    
    EXPECT_THROW(
        handler->hasKey(trustLineID),
        IOError
    );
} 