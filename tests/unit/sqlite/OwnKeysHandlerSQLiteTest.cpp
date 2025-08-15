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
#include "../../../src/core/logger/Logger.h"
#include <cstdint>

using namespace std;
using namespace testing;

// Minimal stub factory to generate TrustLineID values
class DummyFactory {
public:
    TrustLineID generateTrustLineID() {
        static TrustLineID sID = 1;
        return sID++;
    }
};

class OwnKeysHandlerSQLiteTest : public Test {
protected:
    void SetUp() override {
        // Initialize libsodium once to enable SecureSegment allocations
        ASSERT_GE(sodium_init(), 0);

        // Create temporary directory for test database
        tempDir = filesystem::temp_directory_path() / "own_keys_test";
        filesystem::create_directories(tempDir);
        
        // Create test database
        testDbPath = tempDir / "test.db";
        int rc = sqlite3_open(testDbPath.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK);
        
        // Create Logger
        logger = make_unique<Logger>();
        
        // Create test data factory
        testDataFactory = make_unique<DummyFactory>();
        
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
    PublicKey::Shared derivePublicKeyFrom(const PrivateKey &privateKey) {
        return privateKey.derivePublicKey();
    }
    
    PrivateKey* generateTestPrivateKey() {
        return new PrivateKey();
    }
    
    KeyHash::Shared generateTestKeyHash() {
        byte_t hashData[KeyHash::kBytesSize];
        fill(hashData, hashData + KeyHash::kBytesSize, 0xEF);
        return make_shared<KeyHash>(hashData);
    }
    
    Signature::Shared generateTestSignature() {
        constexpr size_t kSize = Signature::signatureSize();
        byte_t signatureData[kSize];
        fill(signatureData, signatureData + kSize, 0x12);
        return make_shared<Signature>(signatureData);
    }
    
    filesystem::path tempDir;
    filesystem::path testDbPath;
    sqlite3* db = nullptr;
    unique_ptr<Logger> logger;
    unique_ptr<DummyFactory> testDataFactory;
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
    PrivateKey* privateKey = generateTestPrivateKey();
    PublicKey::Shared publicKey = derivePublicKeyFrom(*privateKey);
    EXPECT_NO_THROW(
        handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey)
    );
    
    // Verify key was saved
    EXPECT_TRUE(handler->hasKey(trustLineID));
    auto keys = handler->publicKeysBySetNumber(trustLineID, sequenceNumber);
    EXPECT_EQ(keys.size(), 1u);
    auto priv = handler->getPrivateKey(trustLineID);
    EXPECT_NE(priv, nullptr);
    
    delete privateKey;
}

TEST_F(OwnKeysHandlerSQLiteTest, SaveKey_NullPublicKey_ThrowsException) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PrivateKey* tmp = generateTestPrivateKey();
    PublicKey::Shared publicKey = derivePublicKeyFrom(*tmp);
    EXPECT_THROW(
        handler->saveKey(trustLineID, sequenceNumber, nullptr, nullptr),
        ValueError
    );
    
    delete tmp;
}

TEST_F(OwnKeysHandlerSQLiteTest, SaveKey_NullPrivateKey_ThrowsException) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PrivateKey* tmp = generateTestPrivateKey();
    PublicKey::Shared publicKey = derivePublicKeyFrom(*tmp);
    EXPECT_THROW(
        handler->saveKey(trustLineID, sequenceNumber, publicKey, nullptr),
        ValueError
    );
    delete tmp;
}

TEST_F(OwnKeysHandlerSQLiteTest, SaveKey_MultipleKeys_SavesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    const int numKeys = 5;
    
    for (int i = 0; i < numKeys; ++i) {
        PrivateKey* privateKey = generateTestPrivateKey();
        PublicKey::Shared publicKey = derivePublicKeyFrom(*privateKey);
        EXPECT_NO_THROW(
            handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey)
        );
        
        delete privateKey;
    }
    
    // Verify all keys were saved
    auto keys = handler->publicKeysBySetNumber(trustLineID, sequenceNumber);
    EXPECT_EQ(keys.size(), static_cast<size_t>(numKeys));
}

// maxKeySetSequenceNumber Tests
TEST_F(OwnKeysHandlerSQLiteTest, MaxKeySetSequenceNumber_WithKeys_ReturnsCorrectValue) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    // Save keys with different sequence numbers
    vector<KeyNumber> sequenceNumbers = {1, 3, 5, 7, 9};
    for (KeyNumber seqNum : sequenceNumbers) {
        PrivateKey* privateKey = generateTestPrivateKey();
        PublicKey::Shared publicKey = derivePublicKeyFrom(*privateKey);
        
        handler->saveKey(trustLineID, seqNum, publicKey, privateKey);
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

// getPrivateKey Tests
TEST_F(OwnKeysHandlerSQLiteTest, GetPrivateKey_ExistingKey_ReturnsKey) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PrivateKey* privateKey = generateTestPrivateKey();
    PublicKey::Shared publicKey = derivePublicKeyFrom(*privateKey);
    
    // Save a key
    handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey);
    
    // Get private key
    auto result = handler->getPrivateKey(trustLineID);
    EXPECT_NE(result, nullptr);
    
    delete privateKey;
}

TEST_F(OwnKeysHandlerSQLiteTest, GetPrivateKey_NoAvailableKeys_ThrowsNotFoundError) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_THROW(
        handler->getPrivateKey(trustLineID),
        NotFoundError
    );
}

// invalidKey Tests
TEST_F(OwnKeysHandlerSQLiteTest, InvalidKey_ValidParameters_InvalidatesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PrivateKey* privateKey = generateTestPrivateKey();
    PublicKey::Shared publicKey = derivePublicKeyFrom(*privateKey);
    
    // Save a key
    handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey);
    
    // Verify private key is available
    EXPECT_NE(handler->getPrivateKey(trustLineID), nullptr);
    
    // Invalidate the key
    Signature::Shared signature = generateTestSignature();
    EXPECT_NO_THROW(
        handler->invalidateKey(trustLineID, signature)
    );
    
    // Public key still exists, but private key should no longer correspond to it
    auto afterPriv = handler->getPrivateKey(trustLineID);
    ASSERT_NE(afterPriv, nullptr);
    auto pub = handler->getPublicKey(trustLineID);
    ASSERT_NE(pub, nullptr);
    Signature badSig;
    ASSERT_TRUE(badSig.sign(*afterPriv, "data"));
    EXPECT_FALSE(badSig.verify(*pub, reinterpret_cast<const byte_t*>("data"), 4));
    
    delete privateKey;
}

TEST_F(OwnKeysHandlerSQLiteTest, InvalidKey_NullSignature_ThrowsException) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_THROW(
        handler->invalidateKey(trustLineID, nullptr),
        ValueError
    );
}

// getPublicKey Tests
TEST_F(OwnKeysHandlerSQLiteTest, GetPublicKey_ExistingKey_ReturnsKey) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PrivateKey* privateKey = generateTestPrivateKey();
    PublicKey::Shared publicKey = derivePublicKeyFrom(*privateKey);
    
    // Save a key
    handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey);
    
    // Retrieve the key
    PublicKey::Shared retrievedKey = handler->getPublicKey(trustLineID);
    EXPECT_NE(retrievedKey, nullptr);
    
    delete privateKey;
}

TEST_F(OwnKeysHandlerSQLiteTest, GetPublicKey_NonExistentKey_ThrowsNotFoundError) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_THROW(
        handler->getPublicKey(trustLineID),
        NotFoundError
    );
}

// publicKeysBySetNumber Tests
TEST_F(OwnKeysHandlerSQLiteTest, PublicKeysBySetNumber_WithKeys_ReturnsCorrectCount) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    const int numKeys = 10;
    
    // Save multiple keys
    for (int i = 0; i < numKeys; ++i) {
        PrivateKey* privateKey = generateTestPrivateKey();
        PublicKey::Shared publicKey = derivePublicKeyFrom(*privateKey);
        
        handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey);
        delete privateKey;
    }
    
    EXPECT_TRUE(handler->hasKey(trustLineID));
    auto keys = handler->publicKeysBySetNumber(trustLineID, sequenceNumber);
    EXPECT_EQ(keys.size(), static_cast<size_t>(numKeys));
}

TEST_F(OwnKeysHandlerSQLiteTest, HasKey_NoKeys_ReturnsFalse) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_FALSE(handler->hasKey(trustLineID));
    auto keys = handler->publicKeysBySetNumber(trustLineID, 1);
    EXPECT_TRUE(keys.empty());
}

// removeUnusedKeys Tests
TEST_F(OwnKeysHandlerSQLiteTest, RemoveUnusedKeys_WithKeys_RemovesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    const int numKeys = 5;
    
    // Save multiple keys
    for (int i = 0; i < numKeys; ++i) {
        PrivateKey* privateKey = generateTestPrivateKey();
        PublicKey::Shared publicKey = derivePublicKeyFrom(*privateKey);
        
        handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey);
        delete privateKey;
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

TEST_F(OwnKeysHandlerSQLiteTest, RemoveUnusedKeys_NoKeys_DoesNotThrow) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_NO_THROW(
        handler->deleteKeysByTrustLineID(trustLineID)
    );
}

// deleteKeysByTrustLineID Tests
TEST_F(OwnKeysHandlerSQLiteTest, DeleteKeysByTrustLineID_WithKeys_DeletesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    const int numKeys = 5;
    
    // Save multiple keys
    for (int i = 0; i < numKeys; ++i) {
        PrivateKey* privateKey = generateTestPrivateKey();
        PublicKey::Shared publicKey = derivePublicKeyFrom(*privateKey);
        
        handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey);
        delete privateKey;
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
    EXPECT_TRUE(keysAfter.empty());
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
    PrivateKey* privateKey = generateTestPrivateKey();
    PublicKey::Shared publicKey = derivePublicKeyFrom(*privateKey);
    
    // Save key
    EXPECT_NO_THROW(
        handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey)
    );
    
    // Verify key is available
    EXPECT_TRUE(handler->hasKey(trustLineID));
    
    // Invalidate key
    Signature::Shared signature = generateTestSignature();
    EXPECT_NO_THROW(
        handler->invalidateKey(trustLineID, signature)
    );
    
    // Private key retrieved after invalidation should not match saved public key
    {
        auto afterPriv = handler->getPrivateKey(trustLineID);
        ASSERT_NE(afterPriv, nullptr);
        auto pub = handler->getPublicKey(trustLineID);
        ASSERT_NE(pub, nullptr);
        Signature badSig;
        ASSERT_TRUE(badSig.sign(*afterPriv, "data"));
        EXPECT_FALSE(badSig.verify(*pub, reinterpret_cast<const byte_t*>("data"), 4));
    }
    
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
        PrivateKey* privateKey = generateTestPrivateKey();
        PublicKey::Shared publicKey = derivePublicKeyFrom(*privateKey);
        
        handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey);
        delete privateKey;
    }
    
    // Bulk count
    auto keys = handler->publicKeysBySetNumber(trustLineID, sequenceNumber);
    EXPECT_EQ(keys.size(), static_cast<size_t>(numKeys));
    
    // Bulk delete
    handler->deleteKeysByTrustLineID(trustLineID);
    
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time (15 seconds for 100 operations on PQ crypto)
    EXPECT_LT(duration.count(), 15000);
}

// Error Handling Tests
TEST_F(OwnKeysHandlerSQLiteTest, ErrorHandling_CorruptedDatabase_ThrowsIOError) {
    // Close the database to simulate corruption
    sqlite3_close(db);
    db = nullptr;
    
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    KeyNumber sequenceNumber = 1;
    PrivateKey* privateKey = generateTestPrivateKey();
    PublicKey::Shared publicKey = derivePublicKeyFrom(*privateKey);
    
    // Operations should throw IOError
    EXPECT_THROW(
        handler->saveKey(trustLineID, sequenceNumber, publicKey, privateKey),
        IOError
    );
    
    EXPECT_THROW(
        handler->hasKey(trustLineID),
        IOError
    );
    
    delete privateKey;
} 