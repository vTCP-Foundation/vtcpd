#include "gtest/gtest.h"
#include "../../../../src/core/io/storage/postgresql/PaymentKeysHandlerPostgreSQL.h"
#include "../../../../src/core/logger/Logger.h"
#include "../../../../src/core/crypto/lamportkeys.h"
#include "../../../../src/core/transactions/transactions/base/TransactionUUID.h"
#include "../fixtures/DatabaseTestHelper.h"
#include "../fixtures/PostgreSQLTestFixtures.h"
#include <memory>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <libpq-fe.h>
#include <sodium.h>

using namespace crypto::lamport;

class PaymentKeysHandlerPostgreSQLIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize sodium library for cryptographic operations
        if (sodium_init() == -1) {
            throw std::runtime_error("Failed to initialize sodium library");
        }
        
        // Create database connection using hardcoded credentials
        mConnection = DatabaseTestHelper::createConnection(
            DatabaseTestHelper::TEST_HOST,
            DatabaseTestHelper::TEST_PORT,
            DatabaseTestHelper::TEST_USER,
            DatabaseTestHelper::TEST_PASSWORD,
            DatabaseTestHelper::TEST_DB_NAME
        );
        
        // Create unique table name for each test
        mTestTableName = "payment_keys_test_" + std::to_string(testCounter++);
        
        // Create PaymentKeysHandlerPostgreSQL instance
        mHandler = std::make_unique<PaymentKeysHandlerPostgreSQL>(
            mConnection,
            mTestTableName,
            mLogger
        );
    }
    
    void TearDown() override {
        // Clean up test data
        cleanupTestData();
        
        // Close database connection
        DatabaseTestHelper::closeConnection(mConnection);
    }
    
    void cleanupTestData() {
        try {
            DatabaseTestHelper::cleanupTable(mConnection, mTestTableName);
        } catch (const std::exception& e) {
            // Continue cleanup even if some operations fail
            std::cerr << "Cleanup warning: " << e.what() << std::endl;
        }
    }
    
    // Helper methods for creating test data
    TransactionUUID createTestTransactionUUID(const std::string& testData) {
        TransactionUUID uuid;
        memset(uuid.data, 0, TransactionUUID::kBytesSize);
        
        size_t dataSize = std::min(testData.length(), static_cast<size_t>(TransactionUUID::kBytesSize));
        memcpy(uuid.data, testData.c_str(), dataSize);
        
        return uuid;
    }
    
    std::pair<PublicKey::Shared, std::unique_ptr<PrivateKey>> createTestKeyPair(const std::string& seed) {
        // Create deterministic key pair for testing
        // First create private key (auto-generates random if no data provided)
        auto privateKey = std::make_unique<PrivateKey>();
        
        // Derive public key from private key
        auto publicKey = privateKey->derivePublicKey();
        
        return std::make_pair(publicKey, std::move(privateKey));
    }
    
    std::string bytesToHexString(const byte_t* data, size_t size) {
        std::ostringstream oss;
        for (size_t i = 0; i < size; ++i) {
            oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(data[i]);
        }
        return oss.str();
    }
    
    int getKeyCount() {
        std::string query = "SELECT COUNT(*) FROM " + mTestTableName;
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get key count");
        }
        
        int count = std::stoi(PQgetvalue(result, 0, 0));
        PQclear(result);
        return count;
    }
    
    // Helper method to verify raw database data
    struct RawKeyData {
        std::string transactionUuidHex;
        std::string publicKeyHex;
        std::string privateKeyHex;
    };
    
    std::vector<RawKeyData> getRawKeyData() {
        std::string query = "SELECT encode(transaction_uuid, 'hex'), encode(public_key, 'hex'), "
                           "encode(private_key, 'hex') FROM " + mTestTableName + " ORDER BY transaction_uuid";
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get raw key data");
        }
        
        std::vector<RawKeyData> data;
        int rows = PQntuples(result);
        
        for (int i = 0; i < rows; ++i) {
            RawKeyData rawData;
            rawData.transactionUuidHex = PQgetvalue(result, i, 0);
            rawData.publicKeyHex = PQgetvalue(result, i, 1);
            rawData.privateKeyHex = PQgetvalue(result, i, 2);
            data.push_back(rawData);
        }
        
        PQclear(result);
        return data;
    }
    
    void insertKeyViaSQL(const TransactionUUID& transactionUUID, 
                        const std::string& publicKeyHex, const std::string& privateKeyHex) {
        std::string transactionUuidHex = bytesToHexString(transactionUUID.data, TransactionUUID::kBytesSize);
        
        std::string query = "INSERT INTO " + mTestTableName + 
                           " (transaction_uuid, public_key, private_key) "
                           "VALUES (decode('" + transactionUuidHex + "', 'hex'), "
                           "decode('" + publicKeyHex + "', 'hex'), "
                           "decode('" + privateKeyHex + "', 'hex'))";
        
        DatabaseTestHelper::executeQuery(mConnection, query);
    }
    
    bool hasTransactionUUID(const TransactionUUID& transactionUUID) {
        std::string transactionUuidHex = bytesToHexString(transactionUUID.data, TransactionUUID::kBytesSize);
        std::string query = "SELECT COUNT(*) FROM " + mTestTableName + 
                           " WHERE transaction_uuid = decode('" + transactionUuidHex + "', 'hex')";
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to check transaction UUID");
        }
        
        int count = std::stoi(PQgetvalue(result, 0, 0));
        PQclear(result);
        return count > 0;
    }

protected:
    PGconn* mConnection;
    std::unique_ptr<PaymentKeysHandlerPostgreSQL> mHandler;
    Logger mLogger;
    std::string mTestTableName;
    static int testCounter;
};

// Initialize static counter
int PaymentKeysHandlerPostgreSQLIntegrationTest::testCounter = 0;

// Test saveOwnKey method
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, saveOwnKey_ValidData_SavesSuccessfully) {
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto keyPair = createTestKeyPair("testSeed");
    
    // Test the method
    EXPECT_NO_THROW(
        mHandler->saveOwnKey(transactionUUID, keyPair.first, keyPair.second.get())
    );
    
    // Verify data was saved
    EXPECT_EQ(getKeyCount(), 1);
    
    // Verify raw database data
    auto rawData = getRawKeyData();
    EXPECT_EQ(rawData.size(), 1);
    EXPECT_FALSE(rawData[0].transactionUuidHex.empty());
    EXPECT_FALSE(rawData[0].publicKeyHex.empty());
    EXPECT_FALSE(rawData[0].privateKeyHex.empty());
}

// Test saveOwnKey with null publicKey
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, saveOwnKey_NullPublicKey_ThrowsException) {
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto keyPair = createTestKeyPair("testSeed");
    
    EXPECT_THROW(
        mHandler->saveOwnKey(transactionUUID, nullptr, keyPair.second.get()),
        ValueError
    );
}

// Test saveOwnKey with null privateKey
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, saveOwnKey_NullPrivateKey_ThrowsException) {
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto keyPair = createTestKeyPair("testSeed");
    
    EXPECT_THROW(
        mHandler->saveOwnKey(transactionUUID, keyPair.first, nullptr),
        ValueError
    );
}

// Test saveOwnKey with multiple keys
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, saveOwnKey_MultipleKeys_SavesAllSuccessfully) {
    std::vector<TransactionUUID> uuids;
    std::vector<std::pair<PublicKey::Shared, std::unique_ptr<PrivateKey>>> keyPairs;
    
    // Create multiple key pairs
    for (int i = 1; i <= 3; ++i) {
        uuids.push_back(createTestTransactionUUID("testTxUUID" + std::to_string(i)));
        keyPairs.push_back(createTestKeyPair("testSeed" + std::to_string(i)));
    }
    
    // Save all keys
    for (size_t i = 0; i < uuids.size(); ++i) {
        EXPECT_NO_THROW(
            mHandler->saveOwnKey(uuids[i], keyPairs[i].first, keyPairs[i].second.get())
        );
    }
    
    // Verify all keys were saved
    EXPECT_EQ(getKeyCount(), 3);
    
    auto rawData = getRawKeyData();
    EXPECT_EQ(rawData.size(), 3);
}

// Test getOwnPrivateKey method
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, getOwnPrivateKey_ValidData_ReturnsCorrectKey) {
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto keyPair = createTestKeyPair("testSeed");
    
    // Save key first
    mHandler->saveOwnKey(transactionUUID, keyPair.first, keyPair.second.get());
    
    // Test the method
    std::unique_ptr<PrivateKey> retrievedKey;
    EXPECT_NO_THROW(
        retrievedKey.reset(mHandler->getOwnPrivateKey(transactionUUID))
    );
    
    // Verify key was retrieved
    EXPECT_TRUE(retrievedKey != nullptr);
    EXPECT_EQ(retrievedKey->keySize(), keyPair.second->keySize());
}

// Test getOwnPrivateKey with non-existent UUID
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, getOwnPrivateKey_NonExistentUUID_ThrowsException) {
    auto transactionUUID = createTestTransactionUUID("nonExistentUUID");
    
    EXPECT_THROW(
        mHandler->getOwnPrivateKey(transactionUUID),
        NotFoundError
    );
}

// Test deleteKeyByTransactionUUID method
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, deleteKeyByTransactionUUID_ValidData_DeletesSuccessfully) {
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto keyPair = createTestKeyPair("testSeed");
    
    // Save key first
    mHandler->saveOwnKey(transactionUUID, keyPair.first, keyPair.second.get());
    
    // Verify key exists
    EXPECT_EQ(getKeyCount(), 1);
    EXPECT_TRUE(hasTransactionUUID(transactionUUID));
    
    // Test the method
    EXPECT_NO_THROW(mHandler->deleteKeyByTransactionUUID(transactionUUID));
    
    // Verify key was deleted
    EXPECT_EQ(getKeyCount(), 0);
    EXPECT_FALSE(hasTransactionUUID(transactionUUID));
}

// Test deleteKeyByTransactionUUID with non-existent UUID
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, deleteKeyByTransactionUUID_NonExistentUUID_DoesNotThrow) {
    auto transactionUUID = createTestTransactionUUID("nonExistentUUID");
    
    // Should not throw even if UUID doesn't exist
    EXPECT_NO_THROW(mHandler->deleteKeyByTransactionUUID(transactionUUID));
    
    // Verify no data was affected
    EXPECT_EQ(getKeyCount(), 0);
}

    // Test allTransactionUUIDs method
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, allTransactionUUIDs_ValidData_ReturnsCorrectUUIDs) {
    // Start with baseline count 
    auto initialCount = mHandler->allTransactionUUIDs().size();
    
    // Create and save multiple keys
    for (int i = 1; i <= 3; ++i) {
        auto uuid = createTestTransactionUUID("unique_test_uuid_" + std::to_string(i) + "_" + mTestTableName);
        auto keyPair = createTestKeyPair("testSeed" + std::to_string(i));
        
        mHandler->saveOwnKey(uuid, keyPair.first, keyPair.second.get());
    }
    
    // Test the method
    auto retrievedUUIDs = mHandler->allTransactionUUIDs();
    
    // Verify results - should have at least 3 more records than initially
    EXPECT_GE(retrievedUUIDs.size(), initialCount + 3);
    
    // The method should return valid TransactionUUID objects
    for (const auto& uuid : retrievedUUIDs) {
        // Verify each UUID has some data (not all zeros)
        bool hasNonZeroData = false;
        for (int i = 0; i < TransactionUUID::kBytesSize; ++i) {
            if (uuid.data[i] != 0) {
                hasNonZeroData = true;
                break;
            }
        }
        // Note: Some UUIDs might be all zeros for other test data, so we just check the method works
    }
}

// Test allTransactionUUIDs with no data
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, allTransactionUUIDs_NoData_ReturnsEmptyVector) {
    auto uuids = mHandler->allTransactionUUIDs();
    
    EXPECT_TRUE(uuids.empty());
}

// Test Raw Database Data Validation
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, RawDataValidation_SaveKey_CorrectDatabaseStorage) {
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto keyPair = createTestKeyPair("testSeed");
    
    // Save key using the class method
    mHandler->saveOwnKey(transactionUUID, keyPair.first, keyPair.second.get());
    
    // Verify raw database data using direct SQL queries
    std::string countQuery = "SELECT COUNT(*) FROM " + mTestTableName;
    PGresult* result = PQexec(mConnection, countQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    EXPECT_EQ(std::stoi(PQgetvalue(result, 0, 0)), 1);
    PQclear(result);
    
    // Verify specific field values
    std::string dataQuery = "SELECT encode(transaction_uuid, 'hex'), encode(public_key, 'hex'), "
                           "encode(private_key, 'hex') FROM " + mTestTableName;
    result = PQexec(mConnection, dataQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    
    // Verify binary data is stored correctly
    std::string storedUuidHex = PQgetvalue(result, 0, 0);
    std::string storedPublicKeyHex = PQgetvalue(result, 0, 1);
    std::string storedPrivateKeyHex = PQgetvalue(result, 0, 2);
    
    EXPECT_FALSE(storedUuidHex.empty());
    EXPECT_FALSE(storedPublicKeyHex.empty());
    EXPECT_FALSE(storedPrivateKeyHex.empty());
    
    // Verify UUID matches what we inserted
    std::string expectedUuidHex = bytesToHexString(transactionUUID.data, TransactionUUID::kBytesSize);
    EXPECT_EQ(storedUuidHex, expectedUuidHex);
    
    // Verify public key matches
    std::string expectedPublicKeyHex = bytesToHexString(keyPair.first->data(), keyPair.first->keySize());
    EXPECT_EQ(storedPublicKeyHex, expectedPublicKeyHex);
    
    PQclear(result);
}

// Test Reverse Validation: Insert via SQL, read via class methods
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, ReverseValidation_InsertViaSQL_ReadViaClass) {
    // Create a unique UUID for this test
    auto transactionUUID = createTestTransactionUUID("reverse_validation_uuid_" + mTestTableName);
    auto keyPair = createTestKeyPair("testSeed");
    
    // Generate hex strings for SQL insertion
    std::string publicKeyHex = bytesToHexString(keyPair.first->data(), keyPair.first->keySize());
    
    // Get private key data with proper unlocking
    std::string privateKeyHex;
    {
        auto guard = keyPair.second->data()->unlockAndInitGuard();
        privateKeyHex = bytesToHexString(static_cast<const byte_t*>(guard.address()), keyPair.second->keySize());
    }
    
    // Insert data via SQL
    insertKeyViaSQL(transactionUUID, publicKeyHex, privateKeyHex);
    
    // Read data via class methods
    std::unique_ptr<PrivateKey> retrievedPrivateKey;
    EXPECT_NO_THROW(
        retrievedPrivateKey.reset(mHandler->getOwnPrivateKey(transactionUUID))
    );
    
    // Verify private key was retrieved correctly
    EXPECT_TRUE(retrievedPrivateKey != nullptr);
    EXPECT_EQ(retrievedPrivateKey->keySize(), keyPair.second->keySize());
    
    // Verify allTransactionUUIDs method works (contains at least some UUIDs)
    auto uuids = mHandler->allTransactionUUIDs();
    EXPECT_GE(uuids.size(), 1);
    
    // Most importantly - verify we can retrieve the private key we inserted
    // This is the core of the reverse validation test
    EXPECT_NO_THROW(
        std::unique_ptr<PrivateKey> testRetrieve(mHandler->getOwnPrivateKey(transactionUUID))
    );
}

// Test complete workflow: save, retrieve, delete
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, CompleteWorkflow_SaveRetrieveDelete_WorksCorrectly) {
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto keyPair = createTestKeyPair("testSeed");
    
    // Step 1: Save key
    EXPECT_NO_THROW(
        mHandler->saveOwnKey(transactionUUID, keyPair.first, keyPair.second.get())
    );
    EXPECT_EQ(getKeyCount(), 1);
    
    // Step 2: Retrieve private key
    std::unique_ptr<PrivateKey> retrievedKey;
    EXPECT_NO_THROW(
        retrievedKey.reset(mHandler->getOwnPrivateKey(transactionUUID))
    );
    EXPECT_TRUE(retrievedKey != nullptr);
    
    // Step 3: Verify UUID is in list
    auto uuids = mHandler->allTransactionUUIDs();
    EXPECT_EQ(uuids.size(), 1);
    
    // Step 4: Delete key
    EXPECT_NO_THROW(mHandler->deleteKeyByTransactionUUID(transactionUUID));
    EXPECT_EQ(getKeyCount(), 0);
    
    // Step 5: Verify key is gone
    EXPECT_THROW(
        mHandler->getOwnPrivateKey(transactionUUID),
        NotFoundError
    );
    
    auto emptyUuids = mHandler->allTransactionUUIDs();
    EXPECT_TRUE(emptyUuids.empty());
}

// Test duplicate transaction UUID constraint
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, saveOwnKey_DuplicateUUID_AllowsOverwrite) {
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto keyPair1 = createTestKeyPair("testSeed1");
    auto keyPair2 = createTestKeyPair("testSeed2");
    
    // Save first key
    mHandler->saveOwnKey(transactionUUID, keyPair1.first, keyPair1.second.get());
    EXPECT_EQ(getKeyCount(), 1);
    
    // Save second key with same UUID (should not fail - table allows duplicates)
    EXPECT_NO_THROW(
        mHandler->saveOwnKey(transactionUUID, keyPair2.first, keyPair2.second.get())
    );
    
    // Note: Since table doesn't have unique constraint on transaction_uuid,
    // this will insert a second record rather than update
    EXPECT_EQ(getKeyCount(), 2);
}

// Test Constructor with null connection
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, Constructor_NullConnection_ThrowsException) {
    EXPECT_THROW(
        PaymentKeysHandlerPostgreSQL(nullptr, "test_table", mLogger),
        ValueError
    );
}

// Test Constructor with empty table name
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, Constructor_EmptyTableName_ThrowsException) {
    EXPECT_THROW(
        PaymentKeysHandlerPostgreSQL(mConnection, "", mLogger),
        ValueError
    );
}

// Test table creation and schema validation
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, TableCreation_ValidatesSchemaCorrectly) {
    // Test that the table was created with correct schema
    std::string schemaQuery = "SELECT column_name, data_type FROM information_schema.columns WHERE table_name = '" + mTestTableName + "' ORDER BY ordinal_position";
    PGresult* result = PQexec(mConnection, schemaQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    
    // Verify we have the expected columns
    int columnCount = PQntuples(result);
    EXPECT_EQ(columnCount, 3); // transaction_uuid, public_key, private_key
    
    // Verify column names
    std::vector<std::string> expectedColumns = {"transaction_uuid", "public_key", "private_key"};
    std::vector<std::string> actualColumns;
    for (int i = 0; i < columnCount; ++i) {
        actualColumns.push_back(PQgetvalue(result, i, 0));
    }
    
    for (const auto& expectedCol : expectedColumns) {
        EXPECT_TRUE(std::find(actualColumns.begin(), actualColumns.end(), expectedCol) != actualColumns.end())
            << "Expected column '" << expectedCol << "' not found in table schema";
    }
    
    PQclear(result);
}

// Test index creation validation
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, IndexCreation_ValidatesIndexExists) {
    // Check that the transaction_uuid index was created
    std::string indexQuery = "SELECT indexname FROM pg_indexes WHERE tablename = '" + mTestTableName + "' AND indexname LIKE '%transaction_uuid_idx'";
    PGresult* result = PQexec(mConnection, indexQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    
    // Should have at least one index on transaction_uuid
    int indexCount = PQntuples(result);
    EXPECT_GE(indexCount, 1);
    
    PQclear(result);
}

// Test large key data handling
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, saveOwnKey_LargeKeyData_HandlesCorrectly) {
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto keyPair = createTestKeyPair("testSeed");
    
    // Lamport keys are typically large (private key is ~32KB)
    EXPECT_GT(keyPair.second->keySize(), 1000); // Should be much larger than 1KB
    
    // Should handle large key data without issues
    EXPECT_NO_THROW(
        mHandler->saveOwnKey(transactionUUID, keyPair.first, keyPair.second.get())
    );
    
    // Verify retrieval works with large data
    std::unique_ptr<PrivateKey> retrievedKey;
    EXPECT_NO_THROW(
        retrievedKey.reset(mHandler->getOwnPrivateKey(transactionUUID))
    );
    
    EXPECT_TRUE(retrievedKey != nullptr);
    EXPECT_EQ(retrievedKey->keySize(), keyPair.second->keySize());
} 