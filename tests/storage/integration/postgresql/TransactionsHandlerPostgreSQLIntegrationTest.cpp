#include "gtest/gtest.h"
#include "../../../../src/core/io/storage/postgresql/TransactionsHandlerPostgreSQL.h"
#include "../../../../src/core/logger/Logger.h"
#include "../../../../src/core/transactions/transactions/base/TransactionUUID.h"
#include "../../../../src/core/common/memory/MemoryUtils.h"
#include "../../../../src/core/common/serialization/BytesSerializer.h"
#include "../fixtures/DatabaseTestHelper.h"
#include "../fixtures/PostgreSQLTestFixtures.h"

class TransactionsHandlerPostgreSQLIntegrationTest : public ::testing::Test {
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
        
        // Create TransactionsHandlerPostgreSQL instance
        mHandler = std::make_unique<TransactionsHandlerPostgreSQL>(
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
    
    // Helper methods to create test data
    TransactionUUID createValidTransactionUUID() {
        return TransactionUUID(); // Creates a new random UUID
    }
    
    TransactionUUID createDifferentTransactionUUID() {
        return TransactionUUID(); // Creates another new random UUID
    }
    
    TransactionUUID createTransactionUUIDFromString(const std::string& uuidStr) {
        return TransactionUUID(uuidStr);
    }
    
    BytesShared createValidTransactionData(size_t size = 256) {
        BytesShared data = tryMalloc(size);
        // Fill with deterministic test data
        for (size_t i = 0; i < size; ++i) {
            data.get()[i] = static_cast<uint8_t>((i * 17 + 23) % 256);
        }
        return data;
    }
    
    BytesShared createDifferentTransactionData(size_t size = 512) {
        BytesShared data = tryMalloc(size);
        // Fill with different deterministic test data
        for (size_t i = 0; i < size; ++i) {
            data.get()[i] = static_cast<uint8_t>((i * 13 + 37) % 256);
        }
        return data;
    }
    
    BytesShared createEmptyTransactionData(size_t size = 1) {
        BytesShared data = tryMalloc(size);
        // Fill with zeros
        memset(data.get(), 0, size);
        return data;
    }
    
    // Helper method to verify raw database data
    struct RawTransactionData {
        std::string transactionUuidHex;
        std::string transactionBodyHex;
        int transactionBytesCount;
    };
    
    std::vector<RawTransactionData> getRawTransactionData() {
        std::string query = "SELECT encode(transaction_uuid, 'hex') as uuid_hex, "
                           "encode(transaction_body, 'hex') as body_hex, "
                           "transaction_bytes_count FROM " + mTestTableName + " ORDER BY transaction_uuid";
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get raw transaction data");
        }
        
        std::vector<RawTransactionData> data;
        int rows = PQntuples(result);
        
        for (int i = 0; i < rows; ++i) {
            RawTransactionData rawData;
            rawData.transactionUuidHex = PQgetvalue(result, i, 0);
            rawData.transactionBodyHex = PQgetvalue(result, i, 1);
            rawData.transactionBytesCount = std::stoi(PQgetvalue(result, i, 2));
            data.push_back(rawData);
        }
        
        PQclear(result);
        return data;
    }
    
    RawTransactionData getRawTransactionData(const TransactionUUID& uuid) {
        std::string query = "SELECT encode(transaction_uuid, 'hex') as uuid_hex, "
                           "encode(transaction_body, 'hex') as body_hex, "
                           "transaction_bytes_count FROM " + mTestTableName + " WHERE transaction_uuid = $1";
        const char* params[1];
        int lengths[1];
        int formats[1] = {1};
        BytesSerializer serializer;
        serializer.copy(uuid);
        auto serializedUUID = serializer.collect();
        params[0] = reinterpret_cast<const char*>(serializedUUID.first.get());
        lengths[0] = TransactionUUID::kBytesSize;
        
        PGresult* result = PQexecParams(mConnection, query.c_str(), 1, nullptr, params, lengths, formats, 0);
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get raw transaction data for UUID");
        }
        
        if (PQntuples(result) == 0) {
            PQclear(result);
            throw std::runtime_error("No transaction found with given UUID");
        }
        
        RawTransactionData rawData;
        rawData.transactionUuidHex = PQgetvalue(result, 0, 0);
        rawData.transactionBodyHex = PQgetvalue(result, 0, 1);
        rawData.transactionBytesCount = std::stoi(PQgetvalue(result, 0, 2));
        
        PQclear(result);
        return rawData;
    }
    
    int getTransactionCount() {
        std::string query = "SELECT COUNT(*) FROM " + mTestTableName;
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get transaction count");
        }
        
        int count = std::stoi(PQgetvalue(result, 0, 0));
        PQclear(result);
        return count;
    }
    
    // Helper method to convert hex string to bytes for comparison
    std::vector<uint8_t> hexStringToBytes(const std::string& hex) {
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i < hex.length(); i += 2) {
            std::string byteString = hex.substr(i, 2);
            uint8_t byte = static_cast<uint8_t>(strtol(byteString.c_str(), nullptr, 16));
            bytes.push_back(byte);
        }
        return bytes;
    }
    
    // Helper method to compare BytesShared with raw database data
    bool compareTransactionData(const BytesShared& data, size_t dataSize, const std::string& hexData) {
        if (hexData.length() != dataSize * 2) {
            return false;
        }
        
        std::vector<uint8_t> hexBytes = hexStringToBytes(hexData);
        if (hexBytes.size() != dataSize) {
            return false;
        }
        
        return memcmp(data.get(), hexBytes.data(), dataSize) == 0;
    }
    
    // Helper method to insert transaction data directly via SQL
    void insertTransactionDirectly(const TransactionUUID& uuid, const BytesShared& data, size_t dataSize) {
        std::string query = "INSERT INTO " + mTestTableName + 
                           " (transaction_uuid, transaction_body, transaction_bytes_count) VALUES ($1, $2, $3)";
        const char* params[3];
        int lengths[3];
        int formats[3] = {1, 1, 0};
        
        BytesSerializer serializer;
        serializer.copy(uuid);
        auto serializedUUID = serializer.collect();
        params[0] = reinterpret_cast<const char*>(serializedUUID.first.get());
        lengths[0] = TransactionUUID::kBytesSize;
        
        params[1] = reinterpret_cast<const char*>(data.get());
        lengths[1] = static_cast<int>(dataSize);
        
        std::string countStr = std::to_string(dataSize);
        params[2] = countStr.c_str();
        lengths[2] = 0;
        
        PGresult* result = PQexecParams(mConnection, query.c_str(), 3, nullptr, params, lengths, formats, 0);
        
        if (PQresultStatus(result) != PGRES_COMMAND_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to insert transaction directly");
        }
        
        PQclear(result);
    }
    
    PGconn* mConnection;
    std::unique_ptr<TransactionsHandlerPostgreSQL> mHandler;
    Logger mLogger;
    std::string mTestTableName = "test_transactions";
};

// Test: saveRecord - valid transaction saves successfully
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, saveRecord_ValidTransaction_SavesSuccessfully) {
    // Arrange
    TransactionUUID uuid = createValidTransactionUUID();
    size_t dataSize = 256;
    BytesShared data = createValidTransactionData(dataSize);
    
    // Act
    ASSERT_NO_THROW(mHandler->saveRecord(uuid, data, dataSize));
    
    // Assert
    EXPECT_EQ(getTransactionCount(), 1);
    
    // Verify data can be retrieved
    BytesShared retrievedData = mHandler->getTransaction(uuid);
    EXPECT_NE(retrievedData, nullptr);
    EXPECT_EQ(memcmp(data.get(), retrievedData.get(), dataSize), 0);
}

// Test: saveRecord - multiple transactions save successfully
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, saveRecord_MultipleTransactions_SavesAllSuccessfully) {
    // Arrange
    TransactionUUID uuid1 = createValidTransactionUUID();
    TransactionUUID uuid2 = createDifferentTransactionUUID();
    size_t dataSize1 = 256;
    size_t dataSize2 = 512;
    BytesShared data1 = createValidTransactionData(dataSize1);
    BytesShared data2 = createDifferentTransactionData(dataSize2);
    
    // Act
    ASSERT_NO_THROW(mHandler->saveRecord(uuid1, data1, dataSize1));
    ASSERT_NO_THROW(mHandler->saveRecord(uuid2, data2, dataSize2));
    
    // Assert
    EXPECT_EQ(getTransactionCount(), 2);
    
    // Verify both transactions can be retrieved
    BytesShared retrievedData1 = mHandler->getTransaction(uuid1);
    BytesShared retrievedData2 = mHandler->getTransaction(uuid2);
    
    EXPECT_NE(retrievedData1, nullptr);
    EXPECT_NE(retrievedData2, nullptr);
    
    EXPECT_EQ(memcmp(data1.get(), retrievedData1.get(), dataSize1), 0);
    EXPECT_EQ(memcmp(data2.get(), retrievedData2.get(), dataSize2), 0);
}

// Test: saveRecord - duplicate UUID updates existing record
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, saveRecord_DuplicateUUID_UpdatesExistingRecord) {
    // Arrange
    TransactionUUID uuid = createValidTransactionUUID();
    size_t dataSize1 = 256;
    size_t dataSize2 = 512;
    BytesShared data1 = createValidTransactionData(dataSize1);
    BytesShared data2 = createDifferentTransactionData(dataSize2);
    
    // Act - Save first transaction
    ASSERT_NO_THROW(mHandler->saveRecord(uuid, data1, dataSize1));
    EXPECT_EQ(getTransactionCount(), 1);
    
    // Act - Save second transaction with same UUID
    ASSERT_NO_THROW(mHandler->saveRecord(uuid, data2, dataSize2));
    
    // Assert
    EXPECT_EQ(getTransactionCount(), 1); // Still only one record
    
    // Verify the record was updated with new data
    BytesShared retrievedData = mHandler->getTransaction(uuid);
    EXPECT_NE(retrievedData, nullptr);
    EXPECT_EQ(memcmp(data2.get(), retrievedData.get(), dataSize2), 0);
}

// Test: saveRecord - null transaction data throws ValueError
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, saveRecord_NullTransactionData_ThrowsValueError) {
    // Arrange
    TransactionUUID uuid = createValidTransactionUUID();
    BytesShared nullData = nullptr;
    size_t dataSize = 256;
    
    // Act & Assert
    EXPECT_THROW(mHandler->saveRecord(uuid, nullData, dataSize), ValueError);
}

// Test: saveRecord - zero bytes count throws ValueError
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, saveRecord_ZeroBytesCount_ThrowsValueError) {
    // Arrange
    TransactionUUID uuid = createValidTransactionUUID();
    BytesShared data = createValidTransactionData(256);
    size_t zeroDataSize = 0;
    
    // Act & Assert
    EXPECT_THROW(mHandler->saveRecord(uuid, data, zeroDataSize), ValueError);
}

// Test: getTransaction - existing transaction retrieved successfully
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, getTransaction_ExistingTransaction_RetrievesSuccessfully) {
    // Arrange
    TransactionUUID uuid = createValidTransactionUUID();
    size_t dataSize = 256;
    BytesShared originalData = createValidTransactionData(dataSize);
    
    mHandler->saveRecord(uuid, originalData, dataSize);
    
    // Act
    BytesShared retrievedData = mHandler->getTransaction(uuid);
    
    // Assert
    EXPECT_NE(retrievedData, nullptr);
    EXPECT_EQ(memcmp(originalData.get(), retrievedData.get(), dataSize), 0);
}

// Test: getTransaction - non-existent transaction throws NotFoundError
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, getTransaction_NonExistentTransaction_ThrowsNotFoundError) {
    // Arrange
    TransactionUUID nonExistentUuid = createValidTransactionUUID();
    
    // Act & Assert
    EXPECT_THROW(mHandler->getTransaction(nonExistentUuid), NotFoundError);
}

// Test: isTransactionSerialized - existing transaction returns true
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, isTransactionSerialized_ExistingTransaction_ReturnsTrue) {
    // Arrange
    TransactionUUID uuid = createValidTransactionUUID();
    size_t dataSize = 256;
    BytesShared data = createValidTransactionData(dataSize);
    
    mHandler->saveRecord(uuid, data, dataSize);
    
    // Act
    bool isSerialized = mHandler->isTransactionSerialized(uuid);
    
    // Assert
    EXPECT_TRUE(isSerialized);
}

// Test: isTransactionSerialized - non-existent transaction returns false
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, isTransactionSerialized_NonExistentTransaction_ReturnsFalse) {
    // Arrange
    TransactionUUID nonExistentUuid = createValidTransactionUUID();
    
    // Act
    bool isSerialized = mHandler->isTransactionSerialized(nonExistentUuid);
    
    // Assert
    EXPECT_FALSE(isSerialized);
}

// Test: deleteRecordIfExists - existing transaction deleted successfully
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, deleteRecordIfExists_ExistingTransaction_DeletesSuccessfully) {
    // Arrange
    TransactionUUID uuid = createValidTransactionUUID();
    size_t dataSize = 256;
    BytesShared data = createValidTransactionData(dataSize);
    
    mHandler->saveRecord(uuid, data, dataSize);
    EXPECT_EQ(getTransactionCount(), 1);
    EXPECT_TRUE(mHandler->isTransactionSerialized(uuid));
    
    // Act
    ASSERT_NO_THROW(mHandler->deleteRecordIfExists(uuid));
    
    // Assert
    EXPECT_EQ(getTransactionCount(), 0);
    EXPECT_FALSE(mHandler->isTransactionSerialized(uuid));
}

// Test: deleteRecordIfExists - non-existent transaction does not throw
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, deleteRecordIfExists_NonExistentTransaction_DoesNotThrow) {
    // Arrange
    TransactionUUID nonExistentUuid = createValidTransactionUUID();
    
    // Act & Assert
    EXPECT_NO_THROW(mHandler->deleteRecordIfExists(nonExistentUuid));
    EXPECT_EQ(getTransactionCount(), 0);
}

// Test: allTransactions - no transactions returns empty vector
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, allTransactions_NoTransactions_ReturnsEmptyVector) {
    // Act
    std::vector<BytesShared> allTransactions = mHandler->allTransactions();
    
    // Assert
    EXPECT_TRUE(allTransactions.empty());
}

// Test: allTransactions - single transaction returns correct data
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, allTransactions_SingleTransaction_ReturnsCorrectData) {
    // Arrange
    TransactionUUID uuid = createValidTransactionUUID();
    size_t dataSize = 256;
    BytesShared originalData = createValidTransactionData(dataSize);
    
    mHandler->saveRecord(uuid, originalData, dataSize);
    
    // Act
    std::vector<BytesShared> allTransactions = mHandler->allTransactions();
    
    // Assert
    ASSERT_EQ(allTransactions.size(), 1);
    EXPECT_NE(allTransactions[0], nullptr);
    EXPECT_EQ(memcmp(originalData.get(), allTransactions[0].get(), dataSize), 0);
}

// Test: allTransactions - multiple transactions returns all data
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, allTransactions_MultipleTransactions_ReturnsAllData) {
    // Arrange
    TransactionUUID uuid1 = createValidTransactionUUID();
    TransactionUUID uuid2 = createDifferentTransactionUUID();
    size_t dataSize1 = 256;
    size_t dataSize2 = 512;
    BytesShared data1 = createValidTransactionData(dataSize1);
    BytesShared data2 = createDifferentTransactionData(dataSize2);
    
    mHandler->saveRecord(uuid1, data1, dataSize1);
    mHandler->saveRecord(uuid2, data2, dataSize2);
    
    // Act
    std::vector<BytesShared> allTransactions = mHandler->allTransactions();
    
    // Assert
    ASSERT_EQ(allTransactions.size(), 2);
    
    // Note: Order is not guaranteed, so we need to check both possible orders
    bool foundData1 = false, foundData2 = false;
    for (const auto& transaction : allTransactions) {
        EXPECT_NE(transaction, nullptr);
        if (memcmp(data1.get(), transaction.get(), dataSize1) == 0) {
            foundData1 = true;
        } else if (memcmp(data2.get(), transaction.get(), dataSize2) == 0) {
            foundData2 = true;
        }
    }
    
    EXPECT_TRUE(foundData1);
    EXPECT_TRUE(foundData2);
}

// Test: Raw database data validation
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, saveRecord_ValidatesRawDatabaseData) {
    // Arrange
    TransactionUUID uuid = createValidTransactionUUID();
    size_t dataSize = 256;
    BytesShared data = createValidTransactionData(dataSize);
    
    // Act
    mHandler->saveRecord(uuid, data, dataSize);
    
    // Assert - Check raw database data
    auto rawData = getRawTransactionData(uuid);
    
    // Verify UUID is stored correctly (as hex string)
    EXPECT_EQ(rawData.transactionUuidHex.length(), TransactionUUID::kBytesSize * 2);
    
    // Verify transaction body is stored correctly
    EXPECT_EQ(rawData.transactionBytesCount, static_cast<int>(dataSize));
    EXPECT_EQ(rawData.transactionBodyHex.length(), dataSize * 2);
    
    // Verify transaction data matches
    EXPECT_TRUE(compareTransactionData(data, dataSize, rawData.transactionBodyHex));
}

// Test: Raw database data validation with multiple transactions
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, saveRecord_MultipleTransactions_ValidatesRawDatabaseData) {
    // Arrange
    TransactionUUID uuid1 = createValidTransactionUUID();
    TransactionUUID uuid2 = createDifferentTransactionUUID();
    size_t dataSize1 = 128;
    size_t dataSize2 = 512;
    BytesShared data1 = createValidTransactionData(dataSize1);
    BytesShared data2 = createDifferentTransactionData(dataSize2);
    
    // Act
    mHandler->saveRecord(uuid1, data1, dataSize1);
    mHandler->saveRecord(uuid2, data2, dataSize2);
    
    // Assert - Check raw database data
    auto allRawData = getRawTransactionData();
    ASSERT_EQ(allRawData.size(), 2);
    
    // Verify both transactions are stored correctly
    for (const auto& rawData : allRawData) {
        EXPECT_GT(rawData.transactionUuidHex.length(), 0);
        EXPECT_GT(rawData.transactionBytesCount, 0);
        EXPECT_GT(rawData.transactionBodyHex.length(), 0);
        
        // Verify bytes count matches hex data length
        EXPECT_EQ(rawData.transactionBodyHex.length(), rawData.transactionBytesCount * 2);
    }
}

// Test: Reverse validation - Insert via SQL, read via class methods
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, directInsert_ReadViaClassMethods_DeserializesCorrectly) {
    // Arrange
    TransactionUUID uuid = createValidTransactionUUID();
    size_t dataSize = 256;
    BytesShared originalData = createValidTransactionData(dataSize);
    
    // Act 1 - Insert data directly via SQL
    insertTransactionDirectly(uuid, originalData, dataSize);
    
    // Act 2 - Read via class methods
    EXPECT_TRUE(mHandler->isTransactionSerialized(uuid));
    
    BytesShared retrievedData = mHandler->getTransaction(uuid);
    
    // Assert
    EXPECT_NE(retrievedData, nullptr);
    EXPECT_EQ(memcmp(originalData.get(), retrievedData.get(), dataSize), 0);
    
    // Verify allTransactions also works
    auto allTransactions = mHandler->allTransactions();
    ASSERT_EQ(allTransactions.size(), 1);
    EXPECT_EQ(memcmp(originalData.get(), allTransactions[0].get(), dataSize), 0);
}

// Test: Reverse validation - Multiple inserts via SQL, read via class methods
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, directInsert_MultipleTransactions_ReadViaClassMethods) {
    // Arrange
    TransactionUUID uuid1 = createValidTransactionUUID();
    TransactionUUID uuid2 = createDifferentTransactionUUID();
    size_t dataSize1 = 128;
    size_t dataSize2 = 512;
    BytesShared data1 = createValidTransactionData(dataSize1);
    BytesShared data2 = createDifferentTransactionData(dataSize2);
    
    // Act 1 - Insert data directly via SQL
    insertTransactionDirectly(uuid1, data1, dataSize1);
    insertTransactionDirectly(uuid2, data2, dataSize2);
    
    // Act 2 - Read via class methods
    EXPECT_TRUE(mHandler->isTransactionSerialized(uuid1));
    EXPECT_TRUE(mHandler->isTransactionSerialized(uuid2));
    
    BytesShared retrievedData1 = mHandler->getTransaction(uuid1);
    BytesShared retrievedData2 = mHandler->getTransaction(uuid2);
    
    // Assert
    EXPECT_NE(retrievedData1, nullptr);
    EXPECT_NE(retrievedData2, nullptr);
    
    EXPECT_EQ(memcmp(data1.get(), retrievedData1.get(), dataSize1), 0);
    EXPECT_EQ(memcmp(data2.get(), retrievedData2.get(), dataSize2), 0);
    
    // Verify allTransactions works correctly
    auto allTransactions = mHandler->allTransactions();
    ASSERT_EQ(allTransactions.size(), 2);
    
    // Verify both transactions are present
    bool foundData1 = false, foundData2 = false;
    for (const auto& transaction : allTransactions) {
        EXPECT_NE(transaction, nullptr);
        if (memcmp(data1.get(), transaction.get(), dataSize1) == 0) {
            foundData1 = true;
        } else if (memcmp(data2.get(), transaction.get(), dataSize2) == 0) {
            foundData2 = true;
        }
    }
    
    EXPECT_TRUE(foundData1);
    EXPECT_TRUE(foundData2);
}

// Test: Cross-method validation - Save, query, update, delete workflow
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, crossMethodValidation_SaveQueryUpdateDelete_WorksCorrectly) {
    // Arrange
    TransactionUUID uuid = createValidTransactionUUID();
    size_t dataSize1 = 256;
    size_t dataSize2 = 512;
    BytesShared data1 = createValidTransactionData(dataSize1);
    BytesShared data2 = createDifferentTransactionData(dataSize2);
    
    // Act 1 - Save initial transaction
    mHandler->saveRecord(uuid, data1, dataSize1);
    
    // Verify transaction exists
    EXPECT_TRUE(mHandler->isTransactionSerialized(uuid));
    EXPECT_EQ(getTransactionCount(), 1);
    
    // Act 2 - Get transaction and verify data
    BytesShared retrievedData1 = mHandler->getTransaction(uuid);
    EXPECT_NE(retrievedData1, nullptr);
    EXPECT_EQ(memcmp(data1.get(), retrievedData1.get(), dataSize1), 0);
    
    // Act 3 - Update transaction with new data
    mHandler->saveRecord(uuid, data2, dataSize2);
    
    // Verify update worked
    EXPECT_TRUE(mHandler->isTransactionSerialized(uuid));
    EXPECT_EQ(getTransactionCount(), 1); // Still only one record
    
    BytesShared retrievedData2 = mHandler->getTransaction(uuid);
    EXPECT_NE(retrievedData2, nullptr);
    EXPECT_EQ(memcmp(data2.get(), retrievedData2.get(), dataSize2), 0);
    
    // Act 4 - Verify allTransactions returns updated data
    auto allTransactions = mHandler->allTransactions();
    ASSERT_EQ(allTransactions.size(), 1);
    EXPECT_EQ(memcmp(data2.get(), allTransactions[0].get(), dataSize2), 0);
    
    // Act 5 - Delete transaction
    mHandler->deleteRecordIfExists(uuid);
    
    // Verify deletion worked
    EXPECT_FALSE(mHandler->isTransactionSerialized(uuid));
    EXPECT_EQ(getTransactionCount(), 0);
    
    // Verify allTransactions returns empty vector
    auto allTransactionsAfterDelete = mHandler->allTransactions();
    EXPECT_TRUE(allTransactionsAfterDelete.empty());
}

// Test: Table creation and schema validation
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, TableCreation_ValidatesSchemaCorrectly) {
    // The table should be created during SetUp, verify it exists and has correct schema
    std::string schemaQuery = "SELECT column_name, data_type, is_nullable "
                             "FROM information_schema.columns "
                             "WHERE table_name = '" + mTestTableName + "' "
                             "ORDER BY ordinal_position";
    
    PGresult* result = PQexec(mConnection, schemaQuery.c_str());
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    
    // Verify table has 3 columns
    int columnCount = PQntuples(result);
    EXPECT_EQ(columnCount, 3);
    
    // Verify column definitions
    if (columnCount >= 3) {
        // transaction_uuid BYTEA NOT NULL
        EXPECT_EQ(std::string(PQgetvalue(result, 0, 0)), "transaction_uuid");
        EXPECT_EQ(std::string(PQgetvalue(result, 0, 1)), "bytea");
        EXPECT_EQ(std::string(PQgetvalue(result, 0, 2)), "NO");
        
        // transaction_body BYTEA NOT NULL
        EXPECT_EQ(std::string(PQgetvalue(result, 1, 0)), "transaction_body");
        EXPECT_EQ(std::string(PQgetvalue(result, 1, 1)), "bytea");
        EXPECT_EQ(std::string(PQgetvalue(result, 1, 2)), "NO");
        
        // transaction_bytes_count INTEGER NOT NULL
        EXPECT_EQ(std::string(PQgetvalue(result, 2, 0)), "transaction_bytes_count");
        EXPECT_EQ(std::string(PQgetvalue(result, 2, 1)), "integer");
        EXPECT_EQ(std::string(PQgetvalue(result, 2, 2)), "NO");
    }
    
    PQclear(result);
}

// Test: Constructor error handling
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, Constructor_NullConnection_ThrowsValueError) {
    // Arrange & Act & Assert
    EXPECT_THROW(
        TransactionsHandlerPostgreSQL(nullptr, "test_table", mLogger),
        ValueError
    );
}

// Test: Constructor error handling - empty table name
TEST_F(TransactionsHandlerPostgreSQLIntegrationTest, Constructor_EmptyTableName_ThrowsValueError) {
    // Arrange & Act & Assert
    EXPECT_THROW(
        TransactionsHandlerPostgreSQL(mConnection, "", mLogger),
        ValueError
    );
} 

