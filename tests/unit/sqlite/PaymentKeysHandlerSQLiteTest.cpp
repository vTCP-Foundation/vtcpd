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
#include "../fixtures/TestDataFactory.h"

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
        
        // Create test data factory
        testDataFactory = make_unique<TestDataFactory>();
        
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
    
    TransactionUUID generateTestTransactionUUID() {
        return testDataFactory->generateTransactionUUID();
    }
    
    filesystem::path tempDir;
    filesystem::path testDbPath;
    sqlite3* db = nullptr;
    unique_ptr<Logger> logger;
    unique_ptr<TestDataFactory> testDataFactory;
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
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    
    EXPECT_NO_THROW(
        handler->saveOwnKey(transactionUUID, publicKey, privateKey)
    );
    
    // Verify key was saved
    vector<TransactionUUID> uuids = handler->allTransactionUUIDs();
    EXPECT_EQ(uuids.size(), 1);
    EXPECT_EQ(uuids[0], transactionUUID);
    
    delete privateKey;
}

TEST_F(PaymentKeysHandlerSQLiteTest, SaveOwnKey_NullPublicKey_ThrowsException) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    PrivateKey* privateKey = generateTestPrivateKey();
    
    EXPECT_THROW(
        handler->saveOwnKey(transactionUUID, nullptr, privateKey),
        ValueError
    );
    
    delete privateKey;
}

TEST_F(PaymentKeysHandlerSQLiteTest, SaveOwnKey_NullPrivateKey_ThrowsException) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    PublicKey::Shared publicKey = generateTestPublicKey();
    
    EXPECT_THROW(
        handler->saveOwnKey(transactionUUID, publicKey, nullptr),
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
    vector<TransactionUUID> transactionUUIDs;
    
    for (int i = 0; i < numKeys; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        PublicKey::Shared publicKey = generateTestPublicKey();
        PrivateKey* privateKey = generateTestPrivateKey();
        
        transactionUUIDs.push_back(transactionUUID);
        
        EXPECT_NO_THROW(
            handler->saveOwnKey(transactionUUID, publicKey, privateKey)
        );
        
        delete privateKey;
    }
    
    // Verify all keys were saved
    vector<TransactionUUID> savedUUIDs = handler->allTransactionUUIDs();
    EXPECT_EQ(savedUUIDs.size(), numKeys);
    
    // Verify all UUIDs are present
    for (const auto& uuid : transactionUUIDs) {
        EXPECT_NE(find(savedUUIDs.begin(), savedUUIDs.end(), uuid), savedUUIDs.end());
    }
}

// getOwnPrivateKey Tests
TEST_F(PaymentKeysHandlerSQLiteTest, GetOwnPrivateKey_ExistingKey_ReturnsKey) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    
    // Save key
    handler->saveOwnKey(transactionUUID, publicKey, privateKey);
    
    // Retrieve key
    PrivateKey* retrievedKey = handler->getOwnPrivateKey(transactionUUID);
    EXPECT_NE(retrievedKey, nullptr);
    
    delete privateKey;
    delete retrievedKey;
}

TEST_F(PaymentKeysHandlerSQLiteTest, GetOwnPrivateKey_NonExistentKey_ThrowsNotFoundError) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    
    EXPECT_THROW(
        handler->getOwnPrivateKey(transactionUUID),
        NotFoundError
    );
}

TEST_F(PaymentKeysHandlerSQLiteTest, GetOwnPrivateKey_AfterDeletion_ThrowsNotFoundError) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    
    // Save key
    handler->saveOwnKey(transactionUUID, publicKey, privateKey);
    
    // Delete key
    handler->deleteKeyByTransactionUUID(transactionUUID);
    
    // Try to retrieve key
    EXPECT_THROW(
        handler->getOwnPrivateKey(transactionUUID),
        NotFoundError
    );
    
    delete privateKey;
}

// deleteKeyByTransactionUUID Tests
TEST_F(PaymentKeysHandlerSQLiteTest, DeleteKeyByTransactionUUID_ExistingKey_DeletesSuccessfully) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    
    // Save key
    handler->saveOwnKey(transactionUUID, publicKey, privateKey);
    
    // Verify key exists
    vector<TransactionUUID> uuidsBefore = handler->allTransactionUUIDs();
    EXPECT_EQ(uuidsBefore.size(), 1);
    
    // Delete key
    EXPECT_NO_THROW(
        handler->deleteKeyByTransactionUUID(transactionUUID)
    );
    
    // Verify key is deleted
    vector<TransactionUUID> uuidsAfter = handler->allTransactionUUIDs();
    EXPECT_EQ(uuidsAfter.size(), 0);
    
    delete privateKey;
}

TEST_F(PaymentKeysHandlerSQLiteTest, DeleteKeyByTransactionUUID_NonExistentKey_DoesNotThrow) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    
    EXPECT_NO_THROW(
        handler->deleteKeyByTransactionUUID(transactionUUID)
    );
}

TEST_F(PaymentKeysHandlerSQLiteTest, DeleteKeyByTransactionUUID_MultipleCalls_DoesNotThrow) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    
    // Save key
    handler->saveOwnKey(transactionUUID, publicKey, privateKey);
    
    // Delete multiple times
    EXPECT_NO_THROW(handler->deleteKeyByTransactionUUID(transactionUUID));
    EXPECT_NO_THROW(handler->deleteKeyByTransactionUUID(transactionUUID));
    EXPECT_NO_THROW(handler->deleteKeyByTransactionUUID(transactionUUID));
    
    delete privateKey;
}

// allTransactionUUIDs Tests
TEST_F(PaymentKeysHandlerSQLiteTest, AllTransactionUUIDs_NoKeys_ReturnsEmptyVector) {
    vector<TransactionUUID> uuids = handler->allTransactionUUIDs();
    EXPECT_TRUE(uuids.empty());
}

TEST_F(PaymentKeysHandlerSQLiteTest, AllTransactionUUIDs_WithKeys_ReturnsAllUUIDs) {
    const int numKeys = 5;
    vector<TransactionUUID> expectedUUIDs;
    
    // Save multiple keys
    for (int i = 0; i < numKeys; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        PublicKey::Shared publicKey = generateTestPublicKey();
        PrivateKey* privateKey = generateTestPrivateKey();
        
        expectedUUIDs.push_back(transactionUUID);
        handler->saveOwnKey(transactionUUID, publicKey, privateKey);
        
        delete privateKey;
    }
    
    // Retrieve all UUIDs
    vector<TransactionUUID> retrievedUUIDs = handler->allTransactionUUIDs();
    EXPECT_EQ(retrievedUUIDs.size(), numKeys);
    
    // Verify all expected UUIDs are present
    for (const auto& uuid : expectedUUIDs) {
        EXPECT_NE(find(retrievedUUIDs.begin(), retrievedUUIDs.end(), uuid), retrievedUUIDs.end());
    }
}

TEST_F(PaymentKeysHandlerSQLiteTest, AllTransactionUUIDs_AfterPartialDeletion_ReturnsRemainingUUIDs) {
    const int numKeys = 5;
    vector<TransactionUUID> transactionUUIDs;
    
    // Save multiple keys
    for (int i = 0; i < numKeys; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        PublicKey::Shared publicKey = generateTestPublicKey();
        PrivateKey* privateKey = generateTestPrivateKey();
        
        transactionUUIDs.push_back(transactionUUID);
        handler->saveOwnKey(transactionUUID, publicKey, privateKey);
        
        delete privateKey;
    }
    
    // Delete half of the keys
    for (int i = 0; i < numKeys / 2; ++i) {
        handler->deleteKeyByTransactionUUID(transactionUUIDs[i]);
    }
    
    // Retrieve remaining UUIDs
    vector<TransactionUUID> remainingUUIDs = handler->allTransactionUUIDs();
    EXPECT_EQ(remainingUUIDs.size(), numKeys - numKeys / 2);
    
    // Verify deleted UUIDs are not present
    for (int i = 0; i < numKeys / 2; ++i) {
        EXPECT_EQ(find(remainingUUIDs.begin(), remainingUUIDs.end(), transactionUUIDs[i]), remainingUUIDs.end());
    }
    
    // Verify remaining UUIDs are present
    for (int i = numKeys / 2; i < numKeys; ++i) {
        EXPECT_NE(find(remainingUUIDs.begin(), remainingUUIDs.end(), transactionUUIDs[i]), remainingUUIDs.end());
    }
}

// Integration Tests
TEST_F(PaymentKeysHandlerSQLiteTest, Integration_SaveRetrieveDelete_WorksCorrectly) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    
    // Save key
    EXPECT_NO_THROW(
        handler->saveOwnKey(transactionUUID, publicKey, privateKey)
    );
    
    // Retrieve key
    PrivateKey* retrievedKey = handler->getOwnPrivateKey(transactionUUID);
    EXPECT_NE(retrievedKey, nullptr);
    
    // Verify UUID is in list
    vector<TransactionUUID> uuids = handler->allTransactionUUIDs();
    EXPECT_EQ(uuids.size(), 1);
    EXPECT_EQ(uuids[0], transactionUUID);
    
    // Delete key
    EXPECT_NO_THROW(
        handler->deleteKeyByTransactionUUID(transactionUUID)
    );
    
    // Verify key is deleted
    uuids = handler->allTransactionUUIDs();
    EXPECT_TRUE(uuids.empty());
    
    delete privateKey;
    delete retrievedKey;
}

TEST_F(PaymentKeysHandlerSQLiteTest, Integration_MultipleKeysLifecycle_WorksCorrectly) {
    const int numKeys = 10;
    vector<TransactionUUID> transactionUUIDs;
    vector<PrivateKey*> privateKeys;
    
    // Save multiple keys
    for (int i = 0; i < numKeys; ++i) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        PublicKey::Shared publicKey = generateTestPublicKey();
        PrivateKey* privateKey = generateTestPrivateKey();
        
        transactionUUIDs.push_back(transactionUUID);
        privateKeys.push_back(privateKey);
        
        EXPECT_NO_THROW(
            handler->saveOwnKey(transactionUUID, publicKey, privateKey)
        );
    }
    
    // Verify all keys exist
    vector<TransactionUUID> allUUIDs = handler->allTransactionUUIDs();
    EXPECT_EQ(allUUIDs.size(), numKeys);
    
    // Retrieve all keys
    for (int i = 0; i < numKeys; ++i) {
        PrivateKey* retrievedKey = handler->getOwnPrivateKey(transactionUUIDs[i]);
        EXPECT_NE(retrievedKey, nullptr);
        delete retrievedKey;
    }
    
    // Delete all keys
    for (int i = 0; i < numKeys; ++i) {
        EXPECT_NO_THROW(
            handler->deleteKeyByTransactionUUID(transactionUUIDs[i])
        );
    }
    
    // Verify all keys are deleted
    allUUIDs = handler->allTransactionUUIDs();
    EXPECT_TRUE(allUUIDs.empty());
    
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
        handler->deleteKeyByTransactionUUID(transactionUUIDs[i]);
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
    
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    PublicKey::Shared publicKey = generateTestPublicKey();
    PrivateKey* privateKey = generateTestPrivateKey();
    
    // Operations should throw IOError
    EXPECT_THROW(
        handler->saveOwnKey(transactionUUID, publicKey, privateKey),
        IOError
    );
    
    EXPECT_THROW(
        handler->getOwnPrivateKey(transactionUUID),
        IOError
    );
    
    EXPECT_THROW(
        handler->allTransactionUUIDs(),
        IOError
    );
    
    delete privateKey;
} 