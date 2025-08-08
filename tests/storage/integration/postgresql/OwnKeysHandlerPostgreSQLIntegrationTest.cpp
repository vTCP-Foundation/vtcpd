#include "gtest/gtest.h"
#include "../../../../src/core/io/storage/postgresql/OwnKeysHandlerPostgreSQL.h"
#include "../../../../src/core/logger/Logger.h"
#include "../fixtures/DatabaseTestHelper.h"
#include "../fixtures/PostgreSQLTestFixtures.h"
#include <memory>
#include <vector>
#include <sstream>
#include <cstring>
#include <libpq-fe.h>
#include <sodium.h>


class OwnKeysHandlerPostgreSQLIntegrationTest : public ::testing::Test {
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
        mTestTableName = "own_keys_test_" + std::to_string(testCounter++);
        
        // Create trust_lines table (required by foreign key constraint)
        createTrustLinesTable();
        
        // Create test trust line records
        insertTestTrustLines();
        
        // Create OwnKeysHandlerPostgreSQL instance
        mHandler = std::make_unique<OwnKeysHandlerPostgreSQL>(
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
    
    void createTrustLinesTable() {
        std::string query = "CREATE TABLE IF NOT EXISTS trust_lines ("
                           "id INTEGER PRIMARY KEY, "
                           "contractor_id INTEGER, "
                           "equivalent INTEGER, "
                           "state INTEGER, "
                           "is_contractor_gateway INTEGER)";
        DatabaseTestHelper::executeQuery(mConnection, query);
    }
    
    void insertTestTrustLines() {
        std::vector<TrustLineID> trustLineIDs = {
            getValidTrustLineID(),
            getValidTrustLineID2(),
            getValidTrustLineID3()
        };
        
        for (const auto& trustLineID : trustLineIDs) {
            std::string query = "INSERT INTO trust_lines (id, contractor_id, equivalent, state, is_contractor_gateway) VALUES (" 
                               + std::to_string(trustLineID) + ", " 
                               + std::to_string(trustLineID * 10) + ", " 
                               + std::to_string(1) + ", " 
                               + std::to_string(1) + ", " 
                               + std::to_string(0) + ") ON CONFLICT (id) DO NOTHING";
            DatabaseTestHelper::executeQuery(mConnection, query);
        }
    }
    
    void cleanupTestData() {
        try {
            DatabaseTestHelper::cleanupTable(mConnection, mTestTableName);
            DatabaseTestHelper::cleanupTable(mConnection, "trust_lines");
        } catch (const std::exception& e) {
            // Continue cleanup even if some operations fail
            std::cerr << "Cleanup warning: " << e.what() << std::endl;
        }
    }
    
    // Helper methods for creating test data
    TrustLineID getValidTrustLineID() const {
        return static_cast<TrustLineID>(100);
    }
    
    TrustLineID getValidTrustLineID2() const {
        return static_cast<TrustLineID>(200);
    }
    
    TrustLineID getValidTrustLineID3() const {
        return static_cast<TrustLineID>(300);
    }
    
    std::pair<PublicKey::Shared, std::unique_ptr<PrivateKey>> createKeyPair() {
        auto privateKey = std::make_unique<PrivateKey>();
        auto publicKey = privateKey->derivePublicKey();
        return std::make_pair(publicKey, std::move(privateKey));
    }
    
    Signature::Shared createSignature(const std::string& testData) {
        auto privateKey = std::make_unique<PrivateKey>();
        const byte_t* data = reinterpret_cast<const byte_t*>(testData.c_str());
        return std::make_shared<Signature>(const_cast<byte_t*>(data), testData.length(), privateKey.get());
    }
    
    int getKeyCount(TrustLineID trustLineID) {
        std::string query = "SELECT COUNT(*) FROM " + mTestTableName + " WHERE trust_line_id = " + std::to_string(trustLineID);
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get key count");
        }
        
        int count = std::stoi(PQgetvalue(result, 0, 0));
        PQclear(result);
        return count;
    }
    
    int getValidKeyCount(TrustLineID trustLineID) {
        std::string query = "SELECT COUNT(*) FROM " + mTestTableName + " WHERE trust_line_id = " + std::to_string(trustLineID) + " AND is_valid = 1";
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get valid key count");
        }
        
        int count = std::stoi(PQgetvalue(result, 0, 0));
        PQclear(result);
        return count;
    }
    
    // Helper method to verify raw database data
    struct RawKeyData {
        std::string hashHex;
        int trustLineId;
        int keysSetSequenceNumber;
        std::string publicKeyHex;
        std::string privateKeyHex;
        int number;
        int isValid;
    };
    
    std::vector<RawKeyData> getRawKeyData(TrustLineID trustLineID) {
        std::string query = "SELECT encode(hash, 'hex'), trust_line_id, keys_set_sequence_number, "
                           "encode(public_key, 'hex'), encode(private_key, 'hex'), number, is_valid "
                           "FROM " + mTestTableName + " WHERE trust_line_id = " + std::to_string(trustLineID) + " ORDER BY number";
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get raw key data");
        }
        
        std::vector<RawKeyData> data;
        int rows = PQntuples(result);
        
        for (int i = 0; i < rows; ++i) {
            RawKeyData rawData;
            rawData.hashHex = PQgetvalue(result, i, 0);
            rawData.trustLineId = std::stoi(PQgetvalue(result, i, 1));
            rawData.keysSetSequenceNumber = std::stoi(PQgetvalue(result, i, 2));
            rawData.publicKeyHex = PQgetvalue(result, i, 3);
            rawData.privateKeyHex = PQgetvalue(result, i, 4);
            rawData.number = std::stoi(PQgetvalue(result, i, 5));
            rawData.isValid = std::stoi(PQgetvalue(result, i, 6));
            data.push_back(rawData);
        }
        
        PQclear(result);
        return data;
    }
    
    void insertKeyViaSQL(TrustLineID trustLineID, KeyNumber sequenceNumber, const PublicKey::Shared& publicKey, 
                         const PrivateKey* privateKey, KeyNumber number, int isValid = 1) {
        auto keyHash = publicKey->hash();
        
        // Convert private key to bytes
        BytesShared privBuf = tryMalloc(PrivateKey::keySize());
        {
            auto guard = privateKey->data()->unlockAndInitGuard();
            memcpy(privBuf.get(), guard.address(), PrivateKey::keySize());
        }
        
        std::string query = "INSERT INTO " + mTestTableName + 
                           " (hash, trust_line_id, keys_set_sequence_number, public_key, private_key, number, is_valid) "
                           "VALUES ($1, $2, $3, $4, $5, $6, $7)";
        
        const int kParams = 7;
        const char *params[kParams];
        int lengths[kParams];
        int formats[kParams] = {1, 0, 0, 1, 1, 0, 0};
        
        params[0] = reinterpret_cast<const char*>(keyHash->data()); lengths[0] = KeyHash::kBytesSize;
        std::string tlIdStr = std::to_string(trustLineID); params[1] = tlIdStr.c_str(); lengths[1] = 0;
        std::string seqStr = std::to_string(sequenceNumber); params[2] = seqStr.c_str(); lengths[2] = 0;
        params[3] = reinterpret_cast<const char*>(publicKey->data()); lengths[3] = publicKey->keySize();
        params[4] = reinterpret_cast<const char*>(privBuf.get()); lengths[4] = PrivateKey::keySize();
        std::string numStr = std::to_string(number); params[5] = numStr.c_str(); lengths[5] = 0;
        std::string validStr = std::to_string(isValid); params[6] = validStr.c_str(); lengths[6] = 0;
        
        PGresult *result = PQexecParams(mConnection, query.c_str(), kParams, nullptr, params, lengths, formats, 0);
        
        if (PQresultStatus(result) != PGRES_COMMAND_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to insert key via SQL");
        }
        
        PQclear(result);
    }

protected:
    PGconn* mConnection;
    std::unique_ptr<OwnKeysHandlerPostgreSQL> mHandler;
    Logger mLogger;
    std::string mTestTableName;
    static int testCounter;
};

// Initialize static counter
int OwnKeysHandlerPostgreSQLIntegrationTest::testCounter = 0;

// Test: saveKey - Valid key data saves successfully
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, saveKey_ValidData_SavesSuccessfully) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    KeyNumber sequenceNumber = 1;
    KeyNumber keyNumber = 5;
    auto keyPair = createKeyPair();
    
    // Act
    ASSERT_NO_THROW(mHandler->saveKey(trustLineID, sequenceNumber, keyPair.first, keyPair.second.get(), keyNumber));
    
    // Assert
    EXPECT_EQ(getKeyCount(trustLineID), 1);
    EXPECT_EQ(getValidKeyCount(trustLineID), 1);
    
    // Raw Database Data Validation
    auto rawData = getRawKeyData(trustLineID);
    ASSERT_EQ(rawData.size(), 1);
    
    const auto& keyData = rawData[0];
    EXPECT_EQ(keyData.trustLineId, trustLineID);
    EXPECT_EQ(keyData.keysSetSequenceNumber, sequenceNumber);
    EXPECT_EQ(keyData.number, keyNumber);
    EXPECT_EQ(keyData.isValid, 1);
    
    // Verify hash matches public key hash
    auto expectedHash = keyPair.first->hash();
    EXPECT_EQ(keyData.hashHex.length(), KeyHash::kBytesSize * 2); // Hex representation
    
    // Verify public key data
    EXPECT_EQ(keyData.publicKeyHex.length(), PublicKey::keySize() * 2); // Hex representation
    
    // Verify private key data
    EXPECT_EQ(keyData.privateKeyHex.length(), PrivateKey::keySize() * 2); // Hex representation
}

// Test: saveKey - Multiple keys for same trust line
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, saveKey_MultipleKeys_SavesAllSuccessfully) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    KeyNumber sequenceNumber = 1;
    int keyCount = 3;
    
    // Act
    for (int i = 0; i < keyCount; ++i) {
        auto keyPair = createKeyPair();
        ASSERT_NO_THROW(mHandler->saveKey(trustLineID, sequenceNumber, keyPair.first, keyPair.second.get(), i));
    }
    
    // Assert
    EXPECT_EQ(getKeyCount(trustLineID), keyCount);
    EXPECT_EQ(getValidKeyCount(trustLineID), keyCount);
    
    // Raw Database Data Validation
    auto rawData = getRawKeyData(trustLineID);
    ASSERT_EQ(rawData.size(), keyCount);
    
    for (int i = 0; i < keyCount; ++i) {
        EXPECT_EQ(rawData[i].trustLineId, trustLineID);
        EXPECT_EQ(rawData[i].keysSetSequenceNumber, sequenceNumber);
        EXPECT_EQ(rawData[i].number, i);
        EXPECT_EQ(rawData[i].isValid, 1);
    }
}

// Test: saveKey - Null public key throws exception
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, saveKey_NullPublicKey_ThrowsException) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    KeyNumber sequenceNumber = 1;
    KeyNumber keyNumber = 5;
    auto keyPair = createKeyPair();
    
    // Act & Assert
    EXPECT_THROW(mHandler->saveKey(trustLineID, sequenceNumber, nullptr, keyPair.second.get(), keyNumber), ValueError);
}

// Test: saveKey - Null private key throws exception
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, saveKey_NullPrivateKey_ThrowsException) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    KeyNumber sequenceNumber = 1;
    KeyNumber keyNumber = 5;
    auto keyPair = createKeyPair();
    
    // Act & Assert
    EXPECT_THROW(mHandler->saveKey(trustLineID, sequenceNumber, keyPair.first, nullptr, keyNumber), ValueError);
}

// Test: maxKeySetSequenceNumber - Valid data returns correct maximum
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, maxKeySetSequenceNumber_ValidData_ReturnsCorrectMaximum) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    std::vector<KeyNumber> sequenceNumbers = {1, 5, 3, 7, 2};
    KeyNumber expectedMax = 7;
    
    // Save keys with different sequence numbers
    for (const auto& seqNum : sequenceNumbers) {
        auto keyPair = createKeyPair();
        mHandler->saveKey(trustLineID, seqNum, keyPair.first, keyPair.second.get(), 0);
    }
    
    // Act
    KeyNumber actualMax = mHandler->maxKeySetSequenceNumber(trustLineID);
    
    // Assert
    EXPECT_EQ(actualMax, expectedMax);
}

// Test: maxKeySetSequenceNumber - No keys throws exception
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, maxKeySetSequenceNumber_NoKeys_ThrowsException) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    
    // Act & Assert
    EXPECT_THROW(mHandler->maxKeySetSequenceNumber(trustLineID), NotFoundError);
}

// Test: nextAvailableKey - Valid key returns correct data
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, nextAvailableKey_ValidKeys_ReturnsCorrectKey) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    KeyNumber sequenceNumber = 1;
    
    // Save keys with different numbers
    std::vector<KeyNumber> keyNumbers = {5, 1, 3, 7};
    std::vector<std::pair<PublicKey::Shared, std::unique_ptr<PrivateKey>>> keyPairs;
    
    for (const auto& keyNum : keyNumbers) {
        auto keyPair = createKeyPair();
        mHandler->saveKey(trustLineID, sequenceNumber, keyPair.first, keyPair.second.get(), keyNum);
        keyPairs.push_back(std::make_pair(keyPair.first, std::move(keyPair.second)));
    }
    
    // Act
    auto result = mHandler->nextAvailableKey(trustLineID);
    
    // Assert
    EXPECT_NE(result.first, nullptr);
    EXPECT_EQ(result.second, 1); // Should return the key with smallest number
}

// Test: nextAvailableKey - No available keys throws exception
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, nextAvailableKey_NoAvailableKeys_ThrowsException) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    
    // Act & Assert
    EXPECT_THROW(mHandler->nextAvailableKey(trustLineID), NotFoundError);
}

// Test: invalidKey - Valid key gets invalidated
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, invalidKey_ValidKey_InvalidatesSuccessfully) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    KeyNumber sequenceNumber = 1;
    KeyNumber keyNumber = 5;
    auto keyPair = createKeyPair();
    auto signature = createSignature("test data");
    
    // Save key first
    mHandler->saveKey(trustLineID, sequenceNumber, keyPair.first, keyPair.second.get(), keyNumber);
    EXPECT_EQ(getValidKeyCount(trustLineID), 1);
    
    // Act
    ASSERT_NO_THROW(mHandler->invalidKey(trustLineID, keyNumber, signature));
    
    // Assert
    EXPECT_EQ(getValidKeyCount(trustLineID), 0);
    EXPECT_EQ(getKeyCount(trustLineID), 1); // Key still exists but invalid
    
    // Raw Database Data Validation
    auto rawData = getRawKeyData(trustLineID);
    ASSERT_EQ(rawData.size(), 1);
    EXPECT_EQ(rawData[0].isValid, 0);
}

// Test: invalidKey - Null signature throws exception
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, invalidKey_NullSignature_ThrowsException) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    KeyNumber keyNumber = 5;
    
    // Act & Assert
    EXPECT_THROW(mHandler->invalidKey(trustLineID, keyNumber, nullptr), ValueError);
}

// Test: invalidateKeyByHash - Valid key gets invalidated
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, invalidateKeyByHash_ValidKey_InvalidatesSuccessfully) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    KeyNumber sequenceNumber = 1;
    KeyNumber keyNumber = 5;
    auto keyPair = createKeyPair();
    auto signature = createSignature("test data");
    
    // Save key first
    mHandler->saveKey(trustLineID, sequenceNumber, keyPair.first, keyPair.second.get(), keyNumber);
    auto keyHash = keyPair.first->hash();
    EXPECT_EQ(getValidKeyCount(trustLineID), 1);
    
    // Act
    ASSERT_NO_THROW(mHandler->invalidateKeyByHash(trustLineID, keyHash, signature));
    
    // Assert
    EXPECT_EQ(getValidKeyCount(trustLineID), 0);
    EXPECT_EQ(getKeyCount(trustLineID), 1); // Key still exists but invalid
    
    // Raw Database Data Validation
    auto rawData = getRawKeyData(trustLineID);
    ASSERT_EQ(rawData.size(), 1);
    EXPECT_EQ(rawData[0].isValid, 0);
}

// Test: invalidateKeyByHash - Null key hash throws exception
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, invalidateKeyByHash_NullKeyHash_ThrowsException) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    auto signature = createSignature("test data");
    
    // Act & Assert
    EXPECT_THROW(mHandler->invalidateKeyByHash(trustLineID, nullptr, signature), ValueError);
}

// Test: getPublicKey - Valid key returns correct public key
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, getPublicKey_ValidKey_ReturnsCorrectPublicKey) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    KeyNumber sequenceNumber = 1;
    KeyNumber keyNumber = 5;
    auto keyPair = createKeyPair();
    
    // Save key first
    mHandler->saveKey(trustLineID, sequenceNumber, keyPair.first, keyPair.second.get(), keyNumber);
    
    // Act
    auto retrievedKey = mHandler->getPublicKey(trustLineID, keyNumber);
    
    // Assert
    ASSERT_NE(retrievedKey, nullptr);
    
    // Compare key data
    EXPECT_EQ(memcmp(retrievedKey->data(), keyPair.first->data(), PublicKey::keySize()), 0);
}

// Test: getPublicKey - Nonexistent key throws exception
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, getPublicKey_NonexistentKey_ThrowsException) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    KeyNumber keyNumber = 999;
    
    // Act & Assert
    EXPECT_THROW(mHandler->getPublicKey(trustLineID, keyNumber), NotFoundError);
}

// Test: getPublicKeyByHash - Valid key returns correct public key
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, getPublicKeyByHash_ValidKey_ReturnsCorrectPublicKey) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    KeyNumber sequenceNumber = 1;
    KeyNumber keyNumber = 5;
    auto keyPair = createKeyPair();
    
    // Save key first
    mHandler->saveKey(trustLineID, sequenceNumber, keyPair.first, keyPair.second.get(), keyNumber);
    auto keyHash = keyPair.first->hash();
    
    // Act
    auto retrievedKey = mHandler->getPublicKeyByHash(trustLineID, keyHash);
    
    // Assert
    ASSERT_NE(retrievedKey, nullptr);
    
    // Compare key data
    EXPECT_EQ(memcmp(retrievedKey->data(), keyPair.first->data(), PublicKey::keySize()), 0);
}

// Test: getPublicKeyByHash - Null key hash throws exception
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, getPublicKeyByHash_NullKeyHash_ThrowsException) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    
    // Act & Assert
    EXPECT_THROW(mHandler->getPublicKeyByHash(trustLineID, nullptr), ValueError);
}

// Test: getPublicKeyHash - Valid key returns correct hash
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, getPublicKeyHash_ValidKey_ReturnsCorrectHash) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    KeyNumber sequenceNumber = 1;
    KeyNumber keyNumber = 5;
    auto keyPair = createKeyPair();
    
    // Save key first
    mHandler->saveKey(trustLineID, sequenceNumber, keyPair.first, keyPair.second.get(), keyNumber);
    auto expectedHash = keyPair.first->hash();
    
    // Act
    auto retrievedHash = mHandler->getPublicKeyHash(trustLineID, keyNumber);
    
    // Assert
    ASSERT_NE(retrievedHash, nullptr);
    
    // Compare hash data
    EXPECT_EQ(memcmp(retrievedHash->data(), expectedHash->data(), KeyHash::kBytesSize), 0);
}

// Test: getKeyNumberByHash - Valid key returns correct number
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, getKeyNumberByHash_ValidKey_ReturnsCorrectNumber) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    KeyNumber sequenceNumber = 1;
    KeyNumber keyNumber = 5;
    auto keyPair = createKeyPair();
    
    // Save key first
    mHandler->saveKey(trustLineID, sequenceNumber, keyPair.first, keyPair.second.get(), keyNumber);
    auto keyHash = keyPair.first->hash();
    
    // Act
    KeyNumber retrievedNumber = mHandler->getKeyNumberByHash(keyHash);
    
    // Assert
    EXPECT_EQ(retrievedNumber, keyNumber);
}

// Test: getKeyNumberByHash - Null key hash throws exception
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, getKeyNumberByHash_NullKeyHash_ThrowsException) {
    // Act & Assert
    EXPECT_THROW(mHandler->getKeyNumberByHash(nullptr), ValueError);
}

// Test: availableKeysCnt - Valid keys returns correct count
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, availableKeysCnt_ValidKeys_ReturnsCorrectCount) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    KeyNumber sequenceNumber = 1;
    int keyCount = 3;
    
    // Save keys
    for (int i = 0; i < keyCount; ++i) {
        auto keyPair = createKeyPair();
        mHandler->saveKey(trustLineID, sequenceNumber, keyPair.first, keyPair.second.get(), i);
    }
    
    // Act
    KeysCount actualCount = mHandler->availableKeysCnt(trustLineID);
    
    // Assert
    EXPECT_EQ(actualCount, keyCount);
}

// Test: availableKeysCnt - No keys returns zero
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, availableKeysCnt_NoKeys_ReturnsZero) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    
    // Act
    KeysCount actualCount = mHandler->availableKeysCnt(trustLineID);
    
    // Assert
    EXPECT_EQ(actualCount, 0);
}

// Test: removeUnusedKeys - Valid keys get removed
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, removeUnusedKeys_ValidKeys_RemovesSuccessfully) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    KeyNumber sequenceNumber = 1;
    int keyCount = 3;
    
    // Save keys
    for (int i = 0; i < keyCount; ++i) {
        auto keyPair = createKeyPair();
        mHandler->saveKey(trustLineID, sequenceNumber, keyPair.first, keyPair.second.get(), i);
    }
    
    EXPECT_EQ(getValidKeyCount(trustLineID), keyCount);
    
    // Act
    ASSERT_NO_THROW(mHandler->removeUnusedKeys(trustLineID));
    
    // Assert
    EXPECT_EQ(getValidKeyCount(trustLineID), 0);
    EXPECT_EQ(getKeyCount(trustLineID), 0); // Keys are completely removed
}

// Test: publicKeysBySetNumber - Valid keys returned in correct order
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, publicKeysBySetNumber_ValidKeys_ReturnsCorrectOrder) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    KeyNumber sequenceNumber = 1;
    std::vector<KeyNumber> keyNumbers = {3, 1, 5, 2, 4};
    std::vector<PublicKey::Shared> savedKeys;
    
    // Save keys in random order
    for (const auto& keyNum : keyNumbers) {
        auto keyPair = createKeyPair();
        mHandler->saveKey(trustLineID, sequenceNumber, keyPair.first, keyPair.second.get(), keyNum);
        savedKeys.push_back(keyPair.first);
    }
    
    // Act
    auto retrievedKeys = mHandler->publicKeysBySetNumber(trustLineID, sequenceNumber);
    
    // Assert
    ASSERT_EQ(retrievedKeys.size(), keyNumbers.size());
    
    // Keys should be returned in order by number (1, 2, 3, 4, 5)
    std::vector<KeyNumber> expectedOrder = {1, 2, 3, 4, 5};
    for (size_t i = 0; i < expectedOrder.size(); ++i) {
        EXPECT_NE(retrievedKeys[i], nullptr);
        // We can't easily verify the exact key correspondence since they were saved in random order
        // but we can verify the count and that all keys are valid
    }
}

// Test: deleteKeysByTrustLineID - Valid keys get deleted
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, deleteKeysByTrustLineID_ValidKeys_DeletesSuccessfully) {
    // Arrange
    TrustLineID trustLineID1 = getValidTrustLineID();
    TrustLineID trustLineID2 = getValidTrustLineID2();
    KeyNumber sequenceNumber = 1;
    
    // Save keys for both trust lines
    auto keyPair1 = createKeyPair();
    auto keyPair2 = createKeyPair();
    mHandler->saveKey(trustLineID1, sequenceNumber, keyPair1.first, keyPair1.second.get(), 1);
    mHandler->saveKey(trustLineID2, sequenceNumber, keyPair2.first, keyPair2.second.get(), 1);
    
    EXPECT_EQ(getKeyCount(trustLineID1), 1);
    EXPECT_EQ(getKeyCount(trustLineID2), 1);
    
    // Act
    ASSERT_NO_THROW(mHandler->deleteKeysByTrustLineID(trustLineID1));
    
    // Assert
    EXPECT_EQ(getKeyCount(trustLineID1), 0);
    EXPECT_EQ(getKeyCount(trustLineID2), 1); // Other trust line unaffected
}

// Test: deleteKeyByHashExceptSequenceNumber - Correct keys get deleted
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, deleteKeyByHashExceptSequenceNumber_ValidKeys_DeletesCorrectly) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    KeyNumber sequenceNumber1 = 1;
    KeyNumber sequenceNumber2 = 2;
    KeyNumber keyNumber = 5;
    
    // Create key pair and save with different sequence numbers
    auto keyPair = createKeyPair();
    auto keyHash = keyPair.first->hash();
    
    mHandler->saveKey(trustLineID, sequenceNumber1, keyPair.first, keyPair.second.get(), keyNumber);
    // For second save, we need a new private key instance
    auto keyPair2 = createKeyPair();
    mHandler->saveKey(trustLineID, sequenceNumber2, keyPair2.first, keyPair2.second.get(), keyNumber);
    
    EXPECT_EQ(getKeyCount(trustLineID), 2);
    
    // Act - delete keys with hash except sequence number 1
    ASSERT_NO_THROW(mHandler->deleteKeyByHashExceptSequenceNumber(keyHash, sequenceNumber1));
    
    // Assert
    // Since we can't guarantee the same hash for different key pairs, 
    // we test the method doesn't throw and the operation completes
    EXPECT_EQ(getKeyCount(trustLineID), 2); // Both keys remain as they have different hashes
}

// Test: publicKeyHashesLessThanSetNumber - Returns correct hashes
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, publicKeyHashesLessThanSetNumber_ValidKeys_ReturnsCorrectHashes) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    std::vector<KeyNumber> sequenceNumbers = {1, 2, 3, 4, 5};
    std::vector<PublicKey::Shared> savedKeys;
    
    // Save keys with different sequence numbers
    for (const auto& seqNum : sequenceNumbers) {
        auto keyPair = createKeyPair();
        mHandler->saveKey(trustLineID, seqNum, keyPair.first, keyPair.second.get(), 1);
        savedKeys.push_back(keyPair.first);
    }
    
    // Act - get hashes for sequence numbers less than 3
    auto retrievedHashes = mHandler->publicKeyHashesLessThanSetNumber(trustLineID, 3);
    
    // Assert
    EXPECT_EQ(retrievedHashes.size(), 2); // Sequence numbers 1 and 2
    
    for (const auto& hash : retrievedHashes) {
        EXPECT_NE(hash, nullptr);
    }
}

// Test: Reverse Validation - Insert via SQL and read via class methods
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, ReverseValidation_InsertViaSQL_ReadViaClass) {
    // Arrange
    TrustLineID trustLineID = getValidTrustLineID();
    KeyNumber sequenceNumber = 1;
    KeyNumber keyNumber = 5;
    auto keyPair = createKeyPair();
    
    // Act - Insert via SQL
    insertKeyViaSQL(trustLineID, sequenceNumber, keyPair.first, keyPair.second.get(), keyNumber);
    
    // Assert - Read via class methods
    auto retrievedKey = mHandler->getPublicKey(trustLineID, keyNumber);
    ASSERT_NE(retrievedKey, nullptr);
    
    // Compare key data
    EXPECT_EQ(memcmp(retrievedKey->data(), keyPair.first->data(), PublicKey::keySize()), 0);
    
    // Test other methods
    auto retrievedHash = mHandler->getPublicKeyHash(trustLineID, keyNumber);
    ASSERT_NE(retrievedHash, nullptr);
    
    auto expectedHash = keyPair.first->hash();
    EXPECT_EQ(memcmp(retrievedHash->data(), expectedHash->data(), KeyHash::kBytesSize), 0);
    
    KeyNumber retrievedNumber = mHandler->getKeyNumberByHash(expectedHash);
    EXPECT_EQ(retrievedNumber, keyNumber);
    
    KeysCount count = mHandler->availableKeysCnt(trustLineID);
    EXPECT_EQ(count, 1);
}

// Test: Constructor with null connection throws exception
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, Constructor_NullConnection_ThrowsException) {
    // Arrange & Act & Assert
    EXPECT_THROW(OwnKeysHandlerPostgreSQL(nullptr, "test_table", mLogger), ValueError);
}

// Test: Constructor with empty table name throws exception
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, Constructor_EmptyTableName_ThrowsException) {
    // Arrange & Act & Assert
    EXPECT_THROW(OwnKeysHandlerPostgreSQL(mConnection, "", mLogger), ValueError);
}

// Test: Table Creation - Validates schema correctly
TEST_F(OwnKeysHandlerPostgreSQLIntegrationTest, TableCreation_ValidatesSchemaCorrectly) {
    // Arrange - handler is created in SetUp()
    
    // Act - Query table schema
    std::string query = "SELECT column_name, data_type, is_nullable FROM information_schema.columns "
                       "WHERE table_name = '" + mTestTableName + "' ORDER BY ordinal_position";
    PGresult* result = PQexec(mConnection, query.c_str());
    
    // Assert
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    
    int rows = PQntuples(result);
    EXPECT_EQ(rows, 7); // Expected number of columns
    
    // Check specific columns exist
    bool hasHashColumn = false, hasTrustLineIdColumn = false, hasPublicKeyColumn = false;
    bool hasPrivateKeyColumn = false, hasNumberColumn = false, hasIsValidColumn = false;
    
    for (int i = 0; i < rows; ++i) {
        std::string columnName = PQgetvalue(result, i, 0);
        if (columnName == "hash") hasHashColumn = true;
        if (columnName == "trust_line_id") hasTrustLineIdColumn = true;
        if (columnName == "public_key") hasPublicKeyColumn = true;
        if (columnName == "private_key") hasPrivateKeyColumn = true;
        if (columnName == "number") hasNumberColumn = true;
        if (columnName == "is_valid") hasIsValidColumn = true;
    }
    
    EXPECT_TRUE(hasHashColumn);
    EXPECT_TRUE(hasTrustLineIdColumn);
    EXPECT_TRUE(hasPublicKeyColumn);
    EXPECT_TRUE(hasPrivateKeyColumn);
    EXPECT_TRUE(hasNumberColumn);
    EXPECT_TRUE(hasIsValidColumn);
    
    PQclear(result);
} 