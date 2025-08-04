#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sqlite3.h>
#include <memory>
#include <string>
#include <sstream>
#include <filesystem>
#include <fstream>

#include "../../../src/core/io/storage/sqlite/OwnKeysHandlerSQLite.h"
#include "../../../src/core/common/exceptions/IOError.h"
#include "../../../src/core/common/exceptions/NotFoundError.h"
#include "../../../src/core/common/exceptions/ValueError.h"
#include "../../../src/core/crypto/lamportkeys.h"
#include "../../../src/core/crypto/lamportscheme.h"
#include "../../../src/core/logger/Logger.h"
#include "../fixtures/TestDataFactory.h"

using namespace std;
using namespace testing;
using namespace crypto::lamport;

class OwnKeysHandlerSQLiteTest : public Test {
protected:
    void SetUp() override {
        // Create temporary directory for test database
        tempDir = filesystem::temp_directory_path() / "own_keys_test";
        filesystem::create_directories(tempDir);
        
        // Create test database
        testDbPath = tempDir / "test.db";
        int rc = sqlite3_open(testDbPath.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK);
        
        // Create Logger
        logger = make_unique<Logger>(Logger::LogLevel::DEBUG);
        
        // Create test data factory
        testDataFactory = make_unique<TestDataFactory>();
        
        // Create handler
        handler = make_unique<OwnKeysHandlerSQLite>(
            db, 
            "own_keys_test", 
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
        // Generate a simple test public key
        byte keyData[kPublicKeySize];
        fill(keyData, keyData + kPublicKeySize, 0xAB);
        return make_shared<PublicKey>(keyData);
    }
    
    PrivateKey* generateTestPrivateKey() {
        // Generate a simple test private key
        byte keyData[kPrivateKeySize];
        fill(keyData, keyData + kPrivateKeySize, 0xCD);
        return new PrivateKey(keyData);
    }
    
    KeyHash::Shared generateTestKeyHash() {
        // Generate a simple test key hash
        byte hashData[kKeyHashSize];
        fill(hashData, hashData + kKeyHashSize, 0xEF);
        return make_shared<KeyHash>(hashData);
    }
    
    Signature::Shared generateTestSignature() {
        // Generate a simple test signature
        byte signatureData[kSignatureSize];
        fill(signatureData, signatureData + kSignatureSize, 0x12);
        return make_shared<Signature>(signatureData);
    }
    
    filesystem::path tempDir;
    filesystem::path testDbPath;
    sqlite3* db = nullptr;
    unique_ptr<Logger> logger;
    unique_ptr<TestDataFactory> testDataFactory;
    unique_ptr<OwnKeysHandlerSQLite> handler;
};

// Constructor Tests
TEST_F(OwnKeysHandlerSQLiteTest, Constructor_ValidParameters_CreatesTableAndIndexes) {
    // Verify table exists
    string query = "SELECT name FROM sqlite_master WHERE type='table' AND name='own_keys_test';";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt);
    EXPECT_EQ(rc, SQLITE_ROW);
    
    string tableName = (char*)sqlite3_column_text(stmt, 0);
    EXPECT_EQ(tableName, "own_keys_test");
    
    sqlite3_finalize(stmt);
    
    // Verify indexes exist
    query = "SELECT name FROM sqlite_master WHERE type='index' AND name LIKE 'own_keys_test%';";
    rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    int indexCount = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        indexCount++;
    }
    EXPECT_GT(indexCount, 0);  // Should have at least one index
    
    sqlite3_finalize(stmt);
}

TEST_F(OwnKeysHandlerSQLiteTest, Constructor_NullDatabase_ThrowsException) {
    EXPECT_THROW(
        OwnKeysHandlerSQLite(nullptr, "test_table", *logger),
        ValueError
    );
}

TEST_F(OwnKeysHandlerSQLiteTest, Constructor_EmptyTableName_ThrowsException) {
    EXPECT_THROW(
        OwnKeysHandlerSQLite(db, "", *logger),
        ValueError
    );
}

// saveKey Tests
TEST_F(OwnKeysHandlerSQLiteTest, SaveKey_ValidParameters_SavesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    KeyNumber keyNumber = 1;
    
    EXPECT_NO_THROW(
        handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey, keyNumber)
    );
    
    // Verify key was saved
    KeysCount count = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(count, 1);
    
    delete privateKey;
}

TEST_F(OwnKeysHandlerSQLiteTest, SaveKey_NullPublicKey_ThrowsException) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PrivateKey* privateKey = generateTestPrivateKey();
    KeyNumber keyNumber = 1;
    
    EXPECT_THROW(
        handler->saveKey(trustLineID, sequenceNumber, nullptr, privateKey, keyNumber),
        ValueError
    );
    
    delete privateKey;
}

TEST_F(OwnKeysHandlerSQLiteTest, SaveKey_NullPrivateKey_ThrowsException) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    KeyNumber keyNumber = 1;
    
    EXPECT_THROW(
        handler->saveKey(trustLineID, sequenceNumber, publicKey, nullptr, keyNumber),
        ValueError
    );
}

TEST_F(OwnKeysHandlerSQLiteTest, SaveKey_MultipleKeys_SavesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    const int numKeys = 5;
    
    for (int i = 0; i < numKeys; ++i) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        PrivateKey* privateKey = generateTestPrivateKey();
        KeyNumber keyNumber = i + 1;
        
        EXPECT_NO_THROW(
            handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey, keyNumber)
        );
        
        delete privateKey;
    }
    
    // Verify all keys were saved
    KeysCount count = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(count, numKeys);
}

// maxKeySetSequenceNumber Tests
TEST_F(OwnKeysHandlerSQLiteTest, MaxKeySetSequenceNumber_WithKeys_ReturnsCorrectValue) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    // Save keys with different sequence numbers
    vector<KeyNumber> sequenceNumbers = {1, 3, 5, 7, 9};
    for (KeyNumber seqNum : sequenceNumbers) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        PrivateKey* privateKey = generateTestPrivateKey();
        
        handler->saveKey(trustLineID, seqNum, publicKey, privateKey, 1);
        delete privateKey;
    }
    
    KeyNumber maxSeqNum = handler->maxKeySetSequenceNumber(trustLineID);
    EXPECT_EQ(maxSeqNum, 9);
}

TEST_F(OwnKeysHandlerSQLiteTest, MaxKeySetSequenceNumber_NoKeys_ThrowsNotFoundError) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_THROW(
        handler->maxKeySetSequenceNumber(trustLineID),
        NotFoundError
    );
}

// nextAvailableKey Tests
TEST_F(OwnKeysHandlerSQLiteTest, NextAvailableKey_WithAvailableKeys_ReturnsKey) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    KeyNumber keyNumber = 1;
    
    // Save a key
    handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey, keyNumber);
    
    // Get next available key
    auto result = handler->nextAvailableKey(trustLineID);
    EXPECT_NE(result.first, nullptr);
    EXPECT_EQ(result.second, keyNumber);
    
    delete privateKey;
}

TEST_F(OwnKeysHandlerSQLiteTest, NextAvailableKey_NoAvailableKeys_ThrowsNotFoundError) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_THROW(
        handler->nextAvailableKey(trustLineID),
        NotFoundError
    );
}

// invalidKey Tests
TEST_F(OwnKeysHandlerSQLiteTest, InvalidKey_ValidParameters_InvalidatesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    KeyNumber keyNumber = 1;
    
    // Save a key
    handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey, keyNumber);
    
    // Verify key is available
    KeysCount countBefore = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(countBefore, 1);
    
    // Invalidate the key
    Signature::Shared signature = generateTestSignature();
    EXPECT_NO_THROW(
        handler->invalidKey(trustLineID, keyNumber, signature)
    );
    
    // Verify key is no longer available
    KeysCount countAfter = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(countAfter, 0);
    
    delete privateKey;
}

TEST_F(OwnKeysHandlerSQLiteTest, InvalidKey_NullSignature_ThrowsException) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber keyNumber = 1;
    
    EXPECT_THROW(
        handler->invalidKey(trustLineID, keyNumber, nullptr),
        ValueError
    );
}

// getPublicKey Tests
TEST_F(OwnKeysHandlerSQLiteTest, GetPublicKey_ExistingKey_ReturnsKey) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    KeyNumber keyNumber = 1;
    
    // Save a key
    handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey, keyNumber);
    
    // Retrieve the key
    PublicKey::Shared retrievedKey = handler->getPublicKey(trustLineID, keyNumber);
    EXPECT_NE(retrievedKey, nullptr);
    
    delete privateKey;
}

TEST_F(OwnKeysHandlerSQLiteTest, GetPublicKey_NonExistentKey_ThrowsNotFoundError) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber keyNumber = 999;
    
    EXPECT_THROW(
        handler->getPublicKey(trustLineID, keyNumber),
        NotFoundError
    );
}

// availableKeysCnt Tests
TEST_F(OwnKeysHandlerSQLiteTest, AvailableKeysCnt_WithKeys_ReturnsCorrectCount) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    const int numKeys = 10;
    
    // Save multiple keys
    for (int i = 0; i < numKeys; ++i) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        PrivateKey* privateKey = generateTestPrivateKey();
        KeyNumber keyNumber = i + 1;
        
        handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey, keyNumber);
        delete privateKey;
    }
    
    KeysCount count = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(count, numKeys);
}

TEST_F(OwnKeysHandlerSQLiteTest, AvailableKeysCnt_NoKeys_ReturnsZero) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    KeysCount count = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(count, 0);
}

// removeUnusedKeys Tests
TEST_F(OwnKeysHandlerSQLiteTest, RemoveUnusedKeys_WithKeys_RemovesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    const int numKeys = 5;
    
    // Save multiple keys
    for (int i = 0; i < numKeys; ++i) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        PrivateKey* privateKey = generateTestPrivateKey();
        KeyNumber keyNumber = i + 1;
        
        handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey, keyNumber);
        delete privateKey;
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

TEST_F(OwnKeysHandlerSQLiteTest, RemoveUnusedKeys_NoKeys_DoesNotThrow) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_NO_THROW(
        handler->removeUnusedKeys(trustLineID)
    );
}

// deleteKeysByTrustLineID Tests
TEST_F(OwnKeysHandlerSQLiteTest, DeleteKeysByTrustLineID_WithKeys_DeletesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    const int numKeys = 5;
    
    // Save multiple keys
    for (int i = 0; i < numKeys; ++i) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        PrivateKey* privateKey = generateTestPrivateKey();
        KeyNumber keyNumber = i + 1;
        
        handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey, keyNumber);
        delete privateKey;
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

TEST_F(OwnKeysHandlerSQLiteTest, DeleteKeysByTrustLineID_NoKeys_DoesNotThrow) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_NO_THROW(
        handler->deleteKeysByTrustLineID(trustLineID)
    );
}

// Integration Tests
TEST_F(OwnKeysHandlerSQLiteTest, Integration_SaveInvalidateDelete_WorksCorrectly) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    KeyNumber keyNumber = 1;
    
    // Save key
    EXPECT_NO_THROW(
        handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey, keyNumber)
    );
    
    // Verify key is available
    KeysCount count = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(count, 1);
    
    // Invalidate key
    Signature::Shared signature = generateTestSignature();
    EXPECT_NO_THROW(
        handler->invalidKey(trustLineID, keyNumber, signature)
    );
    
    // Verify key is no longer available
    count = handler->availableKeysCnt(trustLineID);
    EXPECT_EQ(count, 0);
    
    // Delete all keys
    EXPECT_NO_THROW(
        handler->deleteKeysByTrustLineID(trustLineID)
    );
    
    delete privateKey;
}

// Performance Tests
TEST_F(OwnKeysHandlerSQLiteTest, Performance_BulkOperations_CompletesInReasonableTime) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    const int numKeys = 100;
    
    auto start = chrono::high_resolution_clock::now();
    
    // Bulk save
    for (int i = 0; i < numKeys; ++i) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        PrivateKey* privateKey = generateTestPrivateKey();
        KeyNumber keyNumber = i + 1;
        
        handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey, keyNumber);
        delete privateKey;
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
TEST_F(OwnKeysHandlerSQLiteTest, ErrorHandling_CorruptedDatabase_ThrowsIOError) {
    // Close the database to simulate corruption
    sqlite3_close(db);
    db = nullptr;
    
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    KeyNumber keyNumber = 1;
    
    // Operations should throw IOError
    EXPECT_THROW(
        handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey, keyNumber),
        IOError
    );
    
    EXPECT_THROW(
        handler->availableKeysCnt(trustLineID),
        IOError
    );
    
    delete privateKey;
} 