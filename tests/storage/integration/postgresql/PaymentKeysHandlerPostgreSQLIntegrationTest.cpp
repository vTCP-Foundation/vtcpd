#include "gtest/gtest.h"
#include "../../../../src/core/io/storage/postgresql/PaymentKeysHandlerPostgreSQL.h"
#include "../../../../src/core/logger/Logger.h"
#include "../fixtures/DatabaseTestHelper.h"
#include <memory>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <libpq-fe.h>


class PaymentKeysHandlerPostgreSQLIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
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
        
        // Force drop existing table to ensure clean schema
        std::string dropQuery = "DROP TABLE IF EXISTS " + mTestTableName + " CASCADE;";
        DatabaseTestHelper::executeQuery(mConnection, dropQuery);
        
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
        uint64_t id;
        std::string publicKeyHex;
        std::string privateKeyHex;
    };
    
    std::vector<RawKeyData> getRawKeyData() {
        std::string query = "SELECT id, encode(public_key, 'hex'), encode(private_key, 'hex') FROM " + mTestTableName + " ORDER BY id";
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get raw key data");
        }
        
        std::vector<RawKeyData> data;
        int rows = PQntuples(result);
        
        for (int i = 0; i < rows; ++i) {
            RawKeyData rawData;
            rawData.id = static_cast<uint64_t>(strtoull(PQgetvalue(result, i, 0), nullptr, 10));
            rawData.publicKeyHex = PQgetvalue(result, i, 1);
            rawData.privateKeyHex = PQgetvalue(result, i, 2);
            data.push_back(rawData);
        }
        
        PQclear(result);
        return data;
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
    auto keyPair = createTestKeyPair("testSeed");
    
    // Test the method
    EXPECT_NO_THROW(
        mHandler->saveOwnKey(keyPair.first, keyPair.second.get())
    );
    
    // Verify data was saved
    EXPECT_EQ(getKeyCount(), 1);
    
    // Verify raw database data
    auto rawData = getRawKeyData();
    EXPECT_EQ(rawData.size(), 1);
    EXPECT_GT(rawData[0].id, 0);
    EXPECT_FALSE(rawData[0].publicKeyHex.empty());
    EXPECT_FALSE(rawData[0].privateKeyHex.empty());
}

// Test saveOwnKey with null publicKey
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, saveOwnKey_NullPublicKey_ThrowsException) {
    auto keyPair = createTestKeyPair("testSeed");
    
    EXPECT_THROW({ mHandler->saveOwnKey(nullptr, keyPair.second.get()); }, std::exception);
}

// Test saveOwnKey with null privateKey
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, saveOwnKey_NullPrivateKey_ThrowsException) {
    auto keyPair = createTestKeyPair("testSeed");
    
    EXPECT_THROW({ mHandler->saveOwnKey(keyPair.first, nullptr); }, std::exception);
}

// Test saveOwnKey with multiple keys
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, saveOwnKey_MultipleKeys_SavesAllSuccessfully) {
    std::vector<std::pair<PublicKey::Shared, std::unique_ptr<PrivateKey>>> keyPairs;
    for (int i = 1; i <= 3; ++i) {
        keyPairs.push_back(createTestKeyPair("testSeed" + std::to_string(i)));
    }
    for (size_t i = 0; i < keyPairs.size(); ++i) {
        EXPECT_NO_THROW(
            mHandler->saveOwnKey(keyPairs[i].first, keyPairs[i].second.get())
        );
    }
    EXPECT_EQ(getKeyCount(), 3);
    auto rawData = getRawKeyData();
    EXPECT_EQ(rawData.size(), 3);
}

// Test getOwnPrivateKey method
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, getOwnPrivateKey_ValidData_ReturnsLatestKey) {
    auto keyPair = createTestKeyPair("testSeed");
    mHandler->saveOwnKey(keyPair.first, keyPair.second.get());
    std::unique_ptr<PrivateKey> retrievedKey;
    EXPECT_NO_THROW(
        retrievedKey.reset(mHandler->getOwnPrivateKey())
    );
    EXPECT_TRUE(retrievedKey != nullptr);
    EXPECT_EQ(retrievedKey->privateKeySize(), PrivateKey::privateKeySize());
}

// Test getOwnPrivateKey with non-existent UUID
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, getOwnPrivateKey_NoKeys_ThrowsNotFound) {
    EXPECT_THROW({ mHandler->getOwnPrivateKey(); }, std::exception);
}

TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, deleteKeyByID_ValidData_DeletesSuccessfully) {
    auto keyPair = createTestKeyPair("testSeed");
    mHandler->saveOwnKey(keyPair.first, keyPair.second.get());
    ASSERT_EQ(getKeyCount(), 1);
    uint64_t id = mHandler->latestKeyID();
    EXPECT_NO_THROW(mHandler->deleteKeyByID(id));
    EXPECT_EQ(getKeyCount(), 0);
}

TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, deleteKeyByID_NonExistent_DoesNotThrow) {
    // Should not throw even if id doesn't exist
    EXPECT_NO_THROW(mHandler->deleteKeyByID(123456789ULL));
    EXPECT_EQ(getKeyCount(), 0);
}

// hasAnyKeys, latestKeyID
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, hasAnyKeysAndLatestID_Workflow) {
    EXPECT_FALSE(mHandler->hasAnyKeys());
    auto keyPair = createTestKeyPair("seedA");
    mHandler->saveOwnKey(keyPair.first, keyPair.second.get());
    EXPECT_TRUE(mHandler->hasAnyKeys());
    uint64_t id1 = mHandler->latestKeyID();
    auto keyPair2 = createTestKeyPair("seedB");
    mHandler->saveOwnKey(keyPair2.first, keyPair2.second.get());
    uint64_t id2 = mHandler->latestKeyID();
    EXPECT_GT(id2, id1);
}

// Test allTransactionUUIDs with no data
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, hasAnyKeys_NoData_ReturnsFalse) {
    EXPECT_FALSE(mHandler->hasAnyKeys());
}

// Test Raw Database Data Validation
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, RawDataValidation_SaveKey_CorrectDatabaseStorage) {
    auto keyPair = createTestKeyPair("testSeed");
    mHandler->saveOwnKey(keyPair.first, keyPair.second.get());
    std::string countQuery = "SELECT COUNT(*) FROM " + mTestTableName;
    PGresult* result = PQexec(mConnection, countQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    EXPECT_EQ(std::stoi(PQgetvalue(result, 0, 0)), 1);
    PQclear(result);
    std::string dataQuery = "SELECT encode(public_key, 'hex'), encode(private_key, 'hex') FROM " + mTestTableName;
    result = PQexec(mConnection, dataQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    std::string storedPublicKeyHex = PQgetvalue(result, 0, 0);
    std::string storedPrivateKeyHex = PQgetvalue(result, 0, 1);
    EXPECT_FALSE(storedPublicKeyHex.empty());
    EXPECT_FALSE(storedPrivateKeyHex.empty());
    std::string expectedPublicKeyHex = bytesToHexString(keyPair.first->data(), keyPair.first->keySize());
    EXPECT_EQ(storedPublicKeyHex, expectedPublicKeyHex);
    PQclear(result);
}

// Test latestKeyID ordering
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, ReverseValidation_LatestIDAndRetrieveLatest) {
    auto keyPair1 = createTestKeyPair("seed1");
    auto keyPair2 = createTestKeyPair("seed2");
    mHandler->saveOwnKey(keyPair1.first, keyPair1.second.get());
    uint64_t id1 = mHandler->latestKeyID();
    mHandler->saveOwnKey(keyPair2.first, keyPair2.second.get());
    uint64_t id2 = mHandler->latestKeyID();
    EXPECT_GT(id2, id1);
    std::unique_ptr<PrivateKey> retrieved;
    EXPECT_NO_THROW(retrieved.reset(mHandler->getOwnPrivateKey()));
    EXPECT_TRUE(retrieved != nullptr);
}

// Test complete workflow: save, retrieve, delete
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, CompleteWorkflow_SaveRetrieveDelete_WorksCorrectly) {
    auto keyPair = createTestKeyPair("testSeed");
    EXPECT_NO_THROW(mHandler->saveOwnKey(keyPair.first, keyPair.second.get()));
    EXPECT_EQ(getKeyCount(), 1);
    std::unique_ptr<PrivateKey> retrievedKey;
    EXPECT_NO_THROW(retrievedKey.reset(mHandler->getOwnPrivateKey()));
    EXPECT_TRUE(retrievedKey != nullptr);
    uint64_t id = mHandler->latestKeyID();
    EXPECT_NO_THROW(mHandler->deleteKeyByID(id));
    EXPECT_EQ(getKeyCount(), 0);
    EXPECT_THROW(mHandler->getOwnPrivateKey(), NotFoundError);
}

// Saving multiple keys increases count
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, saveOwnKey_MultipleInserts_IncreasesCount) {
    auto keyPair1 = createTestKeyPair("testSeed1");
    auto keyPair2 = createTestKeyPair("testSeed2");
    mHandler->saveOwnKey(keyPair1.first, keyPair1.second.get());
    EXPECT_EQ(getKeyCount(), 1);
    EXPECT_NO_THROW(mHandler->saveOwnKey(keyPair2.first, keyPair2.second.get()));
    EXPECT_EQ(getKeyCount(), 2);
}

// Test Constructor with null connection
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, Constructor_NullConnection_ThrowsException) {
    EXPECT_THROW({ PaymentKeysHandlerPostgreSQL(nullptr, "test_table", mLogger); }, std::exception);
}

// Test Constructor with empty table name
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, Constructor_EmptyTableName_ThrowsException) {
    EXPECT_THROW({ PaymentKeysHandlerPostgreSQL(mConnection, "", mLogger); }, std::exception);
}

// Test table creation and schema validation
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, TableCreation_ValidatesSchemaCorrectly) {
    std::string schemaQuery = "SELECT column_name, data_type FROM information_schema.columns WHERE table_name = '" + mTestTableName + "' ORDER BY ordinal_position";
    PGresult* result = PQexec(mConnection, schemaQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    int columnCount = PQntuples(result);
    EXPECT_EQ(columnCount, 3); // id, public_key, private_key
    std::vector<std::string> expectedColumns = {"id", "public_key", "private_key"};
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
    std::string indexQuery = "SELECT indexname FROM pg_indexes WHERE tablename = '" + mTestTableName + "' AND indexname LIKE '%id_idx'";
    PGresult* result = PQexec(mConnection, indexQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    int indexCount = PQntuples(result);
    EXPECT_GE(indexCount, 1);
    PQclear(result);
}

// Basic data handling
TEST_F(PaymentKeysHandlerPostgreSQLIntegrationTest, saveOwnKey_DataRoundtrip_Works) {
    auto keyPair = createTestKeyPair("testSeed");
    EXPECT_NO_THROW(mHandler->saveOwnKey(keyPair.first, keyPair.second.get()));
    std::unique_ptr<PrivateKey> retrievedKey;
    EXPECT_NO_THROW(retrievedKey.reset(mHandler->getOwnPrivateKey()));
    EXPECT_TRUE(retrievedKey != nullptr);
}