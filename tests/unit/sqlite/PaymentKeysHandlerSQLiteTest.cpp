#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sqlite3.h>
#include <memory>
#include <string>
#include <sstream>
#include <filesystem>
#include <fstream>

#include "../../../src/core/io/storage/sqlite/PaymentKeysHandlerSQLite.h"
#include "../../../src/core/common/exceptions/IOError.h"
#include "../../../src/core/common/exceptions/NotFoundError.h"
#include "../../../src/core/common/exceptions/ValueError.h"
#include "../../../src/core/logger/Logger.h"
// Removed TestDataFactory dependency

using namespace std;
using namespace testing;

class PaymentKeysHandlerSQLiteTest : public Test {
protected:
    void SetUp() override {
        // Create temporary directory for test database
        tempDir = filesystem::temp_directory_path() / "payment_keys_test";
        filesystem::create_directories(tempDir);
        
        // Create test database
        testDbPath = tempDir / "test.db";
        int rc = sqlite3_open(testDbPath.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK);
        
        // Create Logger
        logger = make_unique<Logger>(Logger::LogLevel::DEBUG);
        
        // Create handler
        handler = make_unique<PaymentKeysHandlerSQLite>(
            db, 
            "payment_keys_test", 
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
    PublicKey::Shared generateTestPublicKey() {
        // Generate a simple test public key
        byte keyData[PublicKey::keySize()];
        fill(keyData, keyData + PublicKey::keySize(), 0xAB);
        return make_shared<PublicKey>(keyData);
    }
    
    PrivateKey* generateTestPrivateKey() {
        // Generate a simple test private key
        byte keyData[PrivateKey::privateKeySize()];
        fill(keyData, keyData + PrivateKey::privateKeySize(), 0xCD);
        return new PrivateKey(keyData);
    }
    
    filesystem::path tempDir;
    filesystem::path testDbPath;
    sqlite3* db = nullptr;
    unique_ptr<Logger> logger;
    
    unique_ptr<PaymentKeysHandlerSQLite> handler;
};

// Constructor Tests
TEST_F(PaymentKeysHandlerSQLiteTest, Constructor_ValidParameters_CreatesTableAndIndexes) {
    // Verify table exists
    string query = "SELECT name FROM sqlite_master WHERE type='table' AND name='payment_keys_test';";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt);
    EXPECT_EQ(rc, SQLITE_ROW);
    
    string tableName = (char*)sqlite3_column_text(stmt, 0);
    EXPECT_EQ(tableName, "payment_keys_test");
    
    sqlite3_finalize(stmt);
}

TEST_F(PaymentKeysHandlerSQLiteTest, Constructor_NullDatabase_ThrowsException) {
    EXPECT_THROW(
        PaymentKeysHandlerSQLite(nullptr, "test_table", *logger),
        std::exception
    );
}

TEST_F(PaymentKeysHandlerSQLiteTest, Constructor_EmptyTableName_ThrowsException) {
    EXPECT_THROW(
        PaymentKeysHandlerSQLite(db, "", *logger),
        std::exception
    );
}

// saveOwnKey Tests
TEST_F(PaymentKeysHandlerSQLiteTest, SaveOwnKey_ValidParameters_SavesSuccessfully) {
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    
    EXPECT_NO_THROW(
        handler->saveOwnKey(publicKey, privateKey)
    );
    
    // Verify key was saved
    EXPECT_TRUE(handler->hasAnyKeys());
    
    delete privateKey;
}

TEST_F(PaymentKeysHandlerSQLiteTest, SaveOwnKey_NullPublicKey_ThrowsException) {
    PrivateKey* privateKey = generateTestPrivateKey();
    
    EXPECT_THROW(
        handler->saveOwnKey(nullptr, privateKey),
        ValueError
    );
    
    delete privateKey;
}

TEST_F(PaymentKeysHandlerSQLiteTest, SaveOwnKey_NullPrivateKey_ThrowsException) {
    PublicKey::Shared publicKey = generateTestPublicKey();
    
    EXPECT_THROW(
        handler->saveOwnKey(publicKey, nullptr),
        ValueError
    );
}

TEST_F(PaymentKeysHandlerSQLiteTest, SaveOwnKey_DuplicateTransactionUUID_ThrowsException) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey1 = generateTestPrivateKey();
    PrivateKey* privateKey2 = generateTestPrivateKey();
    
    // Save first key
    EXPECT_NO_THROW(
        handler->saveOwnKey(transactionUUID, publicKey, privateKey1)
    );
    
    // Try to save duplicate
    EXPECT_THROW(
        handler->saveOwnKey(transactionUUID, publicKey, privateKey2),
        IOError
    );
    
    delete privateKey1;
    delete privateKey2;
}

TEST_F(PaymentKeysHandlerSQLiteTest, SaveOwnKey_MultipleKeys_SavesSuccessfully) {
    const int numKeys = 5;
    
    for (int i = 0; i < numKeys; ++i) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        PrivateKey* privateKey = generateTestPrivateKey();
        
        EXPECT_NO_THROW(
            handler->saveOwnKey(publicKey, privateKey)
        );
        
        delete privateKey;
    }
    
    EXPECT_TRUE(handler->hasAnyKeys());
    
    // Verify all UUIDs are present
    for (const auto& uuid : transactionUUIDs) {
        EXPECT_NE(find(savedUUIDs.begin(), savedUUIDs.end(), uuid), savedUUIDs.end());
    }
}

// getOwnPrivateKey Tests
TEST_F(PaymentKeysHandlerSQLiteTest, GetOwnPrivateKey_ExistingKey_ReturnsKey) {
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    
    // Save key
    handler->saveOwnKey(publicKey, privateKey);
    
    // Retrieve key
    PrivateKey* retrievedKey = handler->getOwnPrivateKey();
    EXPECT_NE(retrievedKey, nullptr);
    
    delete privateKey;
    delete retrievedKey;
}

TEST_F(PaymentKeysHandlerSQLiteTest, GetOwnPrivateKey_NonExistentKey_ThrowsNotFoundError) {
    EXPECT_THROW(handler->getOwnPrivateKey(), NotFoundError);
}

TEST_F(PaymentKeysHandlerSQLiteTest, GetOwnPrivateKey_AfterDeletion_ThrowsNotFoundError) {
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    
    // Save key
    handler->saveOwnKey(publicKey, privateKey);
    
    // Delete key
    auto id = handler->latestKeyID();
    handler->deleteKeyByID(id);
    
    // Try to retrieve key
    EXPECT_THROW(handler->getOwnPrivateKey(), NotFoundError);
    
    delete privateKey;
}

// deleteKeyByTransactionUUID Tests
TEST_F(PaymentKeysHandlerSQLiteTest, DeleteKeyByID_ExistingKey_DeletesSuccessfully) {
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    
    // Save key
    handler->saveOwnKey(publicKey, privateKey);
    
    // Verify key exists
    EXPECT_TRUE(handler->hasAnyKeys());
    
    // Delete key
    auto id = handler->latestKeyID();
    EXPECT_NO_THROW(handler->deleteKeyByID(id));
    
    // Verify key is deleted
    EXPECT_FALSE(handler->hasAnyKeys());
    
    delete privateKey;
}

TEST_F(PaymentKeysHandlerSQLiteTest, DeleteKeyByID_NonExistentKey_DoesNotThrow) {
    EXPECT_NO_THROW(handler->deleteKeyByID(999999ULL));
}

TEST_F(PaymentKeysHandlerSQLiteTest, DeleteKeyByTransactionUUID_MultipleCalls_DoesNotThrow) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    
    // Save key
    handler->saveOwnKey(transactionUUID, publicKey, privateKey);
    
    // Delete multiple times
    auto id = handler->latestKeyID();
    EXPECT_NO_THROW(handler->deleteKeyByID(id));
    EXPECT_NO_THROW(handler->deleteKeyByID(id));
    EXPECT_NO_THROW(handler->deleteKeyByID(id));
    
    delete privateKey;
}

// allTransactionUUIDs Tests
// hasAnyKeys + latestKeyID
TEST_F(PaymentKeysHandlerSQLiteTest, HasAnyKeysAndLatestID_Workflow) {
    EXPECT_FALSE(handler->hasAnyKeys());
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    handler->saveOwnKey(publicKey, privateKey);
    EXPECT_TRUE(handler->hasAnyKeys());
    auto id = handler->latestKeyID();
    EXPECT_GT(id, 0ULL);
    handler->deleteKeyByID(id);
    EXPECT_FALSE(handler->hasAnyKeys());
    delete privateKey;
}

// Integration Tests
TEST_F(PaymentKeysHandlerSQLiteTest, Integration_SaveRetrieveDelete_WorksCorrectly) {
    
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    
    // Save key
    EXPECT_NO_THROW(
        handler->saveOwnKey(publicKey, privateKey)
    );
    
    // Retrieve key
    PrivateKey* retrievedKey = handler->getOwnPrivateKey();
    EXPECT_NE(retrievedKey, nullptr);
    
    // Delete key
    auto id = handler->latestKeyID();
    EXPECT_NO_THROW(handler->deleteKeyByID(id));
    
    EXPECT_FALSE(handler->hasAnyKeys());
    
    delete privateKey;
    delete retrievedKey;
}

TEST_F(PaymentKeysHandlerSQLiteTest, Integration_MultipleKeysLifecycle_WorksCorrectly) {
    const int numKeys = 10;
    vector<PrivateKey*> privateKeys;
    
    // Save multiple keys
    for (int i = 0; i < numKeys; ++i) {
        PublicKey::Shared publicKey = generateTestPublicKey();
        PrivateKey* privateKey = generateTestPrivateKey();
        
        privateKeys.push_back(privateKey);
        
        EXPECT_NO_THROW(
            handler->saveOwnKey(publicKey, privateKey)
        );
    }
    
    // Verify all keys exist
    EXPECT_TRUE(handler->hasAnyKeys());
    
    // Retrieve all keys
    for (int i = 0; i < numKeys; ++i) {
        PrivateKey* retrievedKey = handler->getOwnPrivateKey();
        EXPECT_NE(retrievedKey, nullptr);
        delete retrievedKey;
    }
    
    // Delete all keys
    for (int i = 0; i < numKeys; ++i) {
        auto id = handler->latestKeyID();
        EXPECT_NO_THROW(handler->deleteKeyByID(id));
    }
    
    // Verify all keys are deleted
    EXPECT_FALSE(handler->hasAnyKeys());
    
    // Cleanup
    for (auto* key : privateKeys) {
        delete key;
    }
}

// Performance Tests
TEST_F(PaymentKeysHandlerSQLiteTest, Performance_BulkOperations_CompletesInReasonableTime) {
    const int numKeys = 100;
    vector<TransactionUUID> transactionUUIDs;
    vector<PrivateKey*> privateKeys;
    
    auto start = chrono::high_resolution_clock::now();
    
    // Bulk save
    for (int i = 0; i < numKeys; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        PublicKey::Shared publicKey = generateTestPublicKey();
        PrivateKey* privateKey = generateTestPrivateKey();
        
        transactionUUIDs.push_back(transactionUUID);
        privateKeys.push_back(privateKey);
        
        handler->saveOwnKey(transactionUUID, publicKey, privateKey);
    }
    
    // Bulk retrieve
    for (int i = 0; i < numKeys; ++i) {
        PrivateKey* retrievedKey = handler->getOwnPrivateKey(transactionUUIDs[i]);
        delete retrievedKey;
    }
    
    // Bulk delete
    for (int i = 0; i < numKeys; ++i) {
        auto id2 = handler->latestKeyID();
        handler->deleteKeyByID(id2);
    }
    
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time (5 seconds for 100 operations)
    EXPECT_LT(duration.count(), 5000);
    
    // Cleanup
    for (auto* key : privateKeys) {
        delete key;
    }
}

// Error Handling Tests
TEST_F(PaymentKeysHandlerSQLiteTest, ErrorHandling_CorruptedDatabase_ThrowsIOError) {
    // Close the database to simulate corruption
    sqlite3_close(db);
    db = nullptr;
    
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    
    // Operations should throw IOError
    EXPECT_THROW(
        handler->saveOwnKey(publicKey, privateKey),
        IOError
    );
    
    EXPECT_THROW(
        handler->getOwnPrivateKey(),
        IOError
    );
    
    delete privateKey;
} 