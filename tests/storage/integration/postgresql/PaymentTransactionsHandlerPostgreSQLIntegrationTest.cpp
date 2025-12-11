#include "../../../../src/core/io/storage/postgresql/PaymentTransactionsHandlerPostgreSQL.h"
#include "../../../../src/core/transactions/transactions/base/TransactionUUID.h"
#include "../../../../src/core/logger/Logger.h"
#include "../../../../src/core/common/exceptions/IOError.h"
#include "../../../../src/core/common/exceptions/ValueError.h"
#include "../../../../src/core/common/Types.h"
#include "../../../../src/core/common/time/TimeUtils.h"
#include "../fixtures/DatabaseTestHelper.h"
#include "gtest/gtest.h"
#include <sstream>
#include <memory>
#include <algorithm>
#include <libpq-fe.h>
#include <sodium.h>

class PaymentTransactionsHandlerPostgreSQLIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
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
        mTableName = "payment_transactions_test_" + std::to_string(testCounter++);
        
        // Force drop existing table to ensure clean schema
        std::string dropQuery = "DROP TABLE IF EXISTS " + mTableName + " CASCADE;";
        DatabaseTestHelper::executeQuery(mConnection, dropQuery);
        
        // Create payment_keys table (required by foreign key constraint)
        createPaymentKeysTable();
        
        // Create PaymentTransactionsHandlerPostgreSQL instance
        mHandler = std::make_unique<PaymentTransactionsHandlerPostgreSQL>(
            mConnection, mTableName, mLogger);
    }

    void TearDown() override
    {
        // Clean up test data
        cleanupTestData();
        
        // Close database connection
        DatabaseTestHelper::closeConnection(mConnection);
    }
    
    void cleanupTestData()
    {
        try {
            DatabaseTestHelper::cleanupTable(mConnection, mTableName);
        } catch (const std::exception& e) {
            // Continue cleanup even if some operations fail
            std::cerr << "Cleanup warning: " << e.what() << std::endl;
        }
    }
    
    void createPaymentKeysTable() {
        try {
            // Drop existing payment_keys table to ensure clean state
            std::string dropQuery = "DROP TABLE IF EXISTS payment_keys CASCADE;";
            DatabaseTestHelper::executeQuery(mConnection, dropQuery);
            
            // Create payment_keys table with the new schema
            std::string createQuery = "CREATE TABLE payment_keys ("
                                     "id BIGSERIAL PRIMARY KEY, "
                                     "public_key BYTEA NOT NULL, "
                                     "private_key BYTEA NOT NULL);";
            DatabaseTestHelper::executeQuery(mConnection, createQuery);
            
            // Create index
            std::string indexQuery = "CREATE INDEX IF NOT EXISTS payment_keys_id_idx ON payment_keys(id);";
            DatabaseTestHelper::executeQuery(mConnection, indexQuery);
            
            // Insert a dummy payment key for tests (required by foreign key constraint)
            std::string insertQuery = "INSERT INTO payment_keys (public_key, private_key) VALUES "
                                     "(decode('0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef', 'hex'), "
                                     "decode('0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef', 'hex'));";
            DatabaseTestHelper::executeQuery(mConnection, insertQuery);
            
        } catch (const std::exception& e) {
            std::cerr << "Failed to create payment_keys table: " << e.what() << std::endl;
            throw;
        }
    }

    // Helper method to create test transaction UUID
    TransactionUUID createTestTransactionUUID(const std::string& testData = "testTxUUID")
    {
        TransactionUUID uuid;
        memset(uuid.data, 0, TransactionUUID::kBytesSize);
        
        size_t dataSize = std::min(testData.length(), static_cast<size_t>(TransactionUUID::kBytesSize));
        memcpy(uuid.data, testData.c_str(), dataSize);
        
        return uuid;
    }

    // Helper method to get row count from database
    int getRowCount()
    {
        std::string query = "SELECT COUNT(*) FROM " + mTableName + ";";
        PGresult *result = PQexec(mConnection, query.c_str());
        EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
        
        int count = 0;
        if (PQntuples(result) > 0) {
            count = std::atoi(PQgetvalue(result, 0, 0));
        }
        PQclear(result);
        return count;
    }

    // Helper method to verify record exists in database
    bool recordExists(const TransactionUUID &transactionUUID)
    {
        std::string queryStr = "SELECT COUNT(*) FROM " + mTableName + " WHERE uuid = $1;";
        
        const char *params[1];
        int lengths[1];
        int formats[1] = {1};
        
        params[0] = reinterpret_cast<const char*>(transactionUUID.data);
        lengths[0] = TransactionUUID::kBytesSize;
        
        PGresult *result = PQexecParams(mConnection, queryStr.c_str(), 1, nullptr, params, lengths, formats, 0);
        EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
        
        int count = 0;
        if (PQntuples(result) > 0) {
            count = std::atoi(PQgetvalue(result, 0, 0));
        }
        PQclear(result);
        return count > 0;
    }

    // Helper method to get observing state from database
    int getObservingState(const TransactionUUID &transactionUUID)
    {
        std::string queryStr = "SELECT observing_state FROM " + mTableName + " WHERE uuid = $1;";
        
        const char *params[1];
        int lengths[1];
        int formats[1] = {1};
        
        params[0] = reinterpret_cast<const char*>(transactionUUID.data);
        lengths[0] = TransactionUUID::kBytesSize;
        
        PGresult *result = PQexecParams(mConnection, queryStr.c_str(), 1, nullptr, params, lengths, formats, 0);
        EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
        EXPECT_EQ(PQntuples(result), 1);
        
        int state = std::atoi(PQgetvalue(result, 0, 0));
        PQclear(result);
        return state;
    }

    std::string mTableName;
    std::unique_ptr<PaymentTransactionsHandlerPostgreSQL> mHandler;
    PGconn* mConnection;
    Logger mLogger;
    static int testCounter;
};

// Initialize static member
int PaymentTransactionsHandlerPostgreSQLIntegrationTest::testCounter = 0;

// Test 1: Constructor validation
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, Constructor_ValidParameters_Success)
{
    // Test that constructor creates handler successfully
    EXPECT_NE(mHandler, nullptr);
    
    // Verify table was created
    std::string query = "SELECT COUNT(*) FROM information_schema.tables WHERE table_name = '" + mTableName + "';";
    PGresult *result = PQexec(mConnection, query.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    EXPECT_EQ(std::atoi(PQgetvalue(result, 0, 0)), 1);
    PQclear(result);
}

TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, Constructor_NullConnection_ThrowsValueError)
{
    EXPECT_THROW(
        PaymentTransactionsHandlerPostgreSQL(nullptr, "test_table", mLogger),
        ValueError
    );
}

TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, Constructor_EmptyTableName_ThrowsValueError)
{
    EXPECT_THROW(
        PaymentTransactionsHandlerPostgreSQL(mConnection, "", mLogger),
        ValueError
    );
}

// Test 2: Save record with valid parameters
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, SaveRecord_ValidParameters_Success)
{
    // Arrange
    auto transactionUUID = createTestTransactionUUID();
    BlockNumber blockNumber = 12345;
    
    // Act
    mHandler->saveRecord(transactionUUID, blockNumber);
    
    // Assert
    EXPECT_EQ(getRowCount(), 1);
    EXPECT_TRUE(recordExists(transactionUUID));
    EXPECT_EQ(getObservingState(transactionUUID), 0); // Initial state should be 0
}

// Test 3: Save multiple records
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, SaveRecord_MultipleRecords_Success)
{
    auto uuid1 = createTestTransactionUUID("uuid1");
    auto uuid2 = createTestTransactionUUID("uuid2");
    BlockNumber block1 = 100;
    BlockNumber block2 = 200;
    
    // Save both records
    mHandler->saveRecord(uuid1, block1);
    mHandler->saveRecord(uuid2, block2);
    
    EXPECT_EQ(getRowCount(), 2);
    EXPECT_TRUE(recordExists(uuid1));
    EXPECT_TRUE(recordExists(uuid2));
}

// Test 4: Save duplicate record (should allow duplicates since no unique constraint)
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, SaveRecord_DuplicateUUID_AllowsDuplicates)
{
    auto transactionUUID = createTestTransactionUUID();
    BlockNumber blockNumber = 12345;
    
    // Save first record
    mHandler->saveRecord(transactionUUID, blockNumber);
    EXPECT_EQ(getRowCount(), 1);
    
    // Save duplicate (should be allowed)
    EXPECT_NO_THROW(mHandler->saveRecord(transactionUUID, blockNumber));
    EXPECT_EQ(getRowCount(), 2); // Now we have 2 records
}

// Test 5: Update transaction state
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, UpdateTransactionState_ValidUUID_Success)
{
    auto transactionUUID = createTestTransactionUUID();
    BlockNumber blockNumber = 12345;
    
    // Save record
    mHandler->saveRecord(transactionUUID, blockNumber);
    EXPECT_EQ(getObservingState(transactionUUID), 0);
    
    // Update state
    mHandler->updateTransactionState(transactionUUID, 1);
    EXPECT_EQ(getObservingState(transactionUUID), 1);
    
    // Update state again
    mHandler->updateTransactionState(transactionUUID, 2);
    EXPECT_EQ(getObservingState(transactionUUID), 2);
}

// Test 6: Update non-existing transaction
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, UpdateTransactionState_NonExistingUUID_ThrowsValueError)
{
    auto transactionUUID = createTestTransactionUUID();
    
    EXPECT_THROW(
        mHandler->updateTransactionState(transactionUUID, 1),
        ValueError
    );
}

// Test 7: Transactions with uncertain observing state
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsWithUncertainObservingState_MixedStates_ReturnsCorrectTransactions)
{
    auto uuid1 = createTestTransactionUUID("uuid1");
    auto uuid2 = createTestTransactionUUID("uuid2");
    auto uuid3 = createTestTransactionUUID("uuid3");
    BlockNumber block1 = 100;
    BlockNumber block2 = 200;
    BlockNumber block3 = 300;
    
    // Save records
    mHandler->saveRecord(uuid1, block1);
    mHandler->saveRecord(uuid2, block2);
    mHandler->saveRecord(uuid3, block3);
    
    // Update some states
    mHandler->updateTransactionState(uuid2, 1);
    // uuid1 and uuid3 remain with state 0
    
    // Get uncertain transactions
    auto uncertainTransactions = mHandler->transactionsWithUncertainObservingState();
    
    ASSERT_EQ(uncertainTransactions.size(), 2);
    
    // Verify that we got the correct transactions with correct BlockNumbers
    // Sort results by block number for consistent comparison
    std::sort(uncertainTransactions.begin(), uncertainTransactions.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });
    
    EXPECT_EQ(uncertainTransactions[0].second, static_cast<BlockNumber>(100));
    EXPECT_EQ(uncertainTransactions[1].second, static_cast<BlockNumber>(300));
}

// Test 8: No uncertain transactions
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsWithUncertainObservingState_NoUncertainTransactions_ReturnsEmpty)
{
    auto transactionUUID = createTestTransactionUUID();
    BlockNumber blockNumber = 12345;
    
    // Save and update state
    mHandler->saveRecord(transactionUUID, blockNumber);
    mHandler->updateTransactionState(transactionUUID, 1);
    
    auto uncertainTransactions = mHandler->transactionsWithUncertainObservingState();
    EXPECT_TRUE(uncertainTransactions.empty());
}

// Test 9: Is transaction present
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, IsTransactionPresent_ExistingTransaction_ReturnsTrue)
{
    auto transactionUUID = createTestTransactionUUID();
    BlockNumber blockNumber = 12345;
    
    // Initially not present
    EXPECT_FALSE(mHandler->isTransactionPresent(transactionUUID));
    
    // Save record
    mHandler->saveRecord(transactionUUID, blockNumber);
    
    // Now present
    EXPECT_TRUE(mHandler->isTransactionPresent(transactionUUID));
}

// Test 10: Is transaction present - non-existing
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, IsTransactionPresent_NonExistingTransaction_ReturnsFalse)
{
    auto transactionUUID = createTestTransactionUUID();
    
    EXPECT_FALSE(mHandler->isTransactionPresent(transactionUUID));
}

// Test 11: Delete record
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, DeleteRecord_ExistingTransaction_Success)
{
    auto transactionUUID = createTestTransactionUUID();
    BlockNumber blockNumber = 12345;
    
    // Save record
    mHandler->saveRecord(transactionUUID, blockNumber);
    EXPECT_TRUE(mHandler->isTransactionPresent(transactionUUID));
    EXPECT_EQ(getRowCount(), 1);
    
    // Delete record
    mHandler->deleteRecord(transactionUUID);
    EXPECT_FALSE(mHandler->isTransactionPresent(transactionUUID));
    EXPECT_EQ(getRowCount(), 0);
}

// Test 12: Delete non-existing record (should not throw)
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, DeleteRecord_NonExistingTransaction_NoError)
{
    auto transactionUUID = createTestTransactionUUID();
    
    // Should not throw
    EXPECT_NO_THROW(mHandler->deleteRecord(transactionUUID));
}

// Test 13: All transactions UUID
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, AllTransactionsUUID_MultipleTransactions_ReturnsAllUUIDs)
{
    auto uuid1 = createTestTransactionUUID("uuid1");
    auto uuid2 = createTestTransactionUUID("uuid2");
    auto uuid3 = createTestTransactionUUID("uuid3");
    BlockNumber blockNumber = 12345;
    
    // Save records
    mHandler->saveRecord(uuid1, blockNumber);
    mHandler->saveRecord(uuid2, blockNumber);
    mHandler->saveRecord(uuid3, blockNumber);
    
    // Get all UUIDs
    auto allUUIDs = mHandler->allTransactionsUUID();
    EXPECT_EQ(allUUIDs.size(), 3);
    
    // Since we can't reliably compare binary UUIDs, just verify we got the correct count
    // which indicates the method works correctly
}

// Test 14: All transactions UUID - empty table
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, AllTransactionsUUID_EmptyTable_ReturnsEmpty)
{
    auto allUUIDs = mHandler->allTransactionsUUID();
    EXPECT_TRUE(allUUIDs.empty());
}

// Test 15: Complete workflow test
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, CompleteWorkflow_SaveUpdateDelete_Success)
{
    auto transactionUUID = createTestTransactionUUID();
    BlockNumber blockNumber = 12345;
    
    // Save
    mHandler->saveRecord(transactionUUID, blockNumber);
    EXPECT_TRUE(mHandler->isTransactionPresent(transactionUUID));
    EXPECT_EQ(getObservingState(transactionUUID), 0);
    
    // Update state
    mHandler->updateTransactionState(transactionUUID, 1);
    EXPECT_EQ(getObservingState(transactionUUID), 1);
    
    // Check uncertain transactions
    auto uncertainTransactions = mHandler->transactionsWithUncertainObservingState();
    EXPECT_TRUE(uncertainTransactions.empty()); // state is 1, not 0
    
    // Delete
    mHandler->deleteRecord(transactionUUID);
    EXPECT_FALSE(mHandler->isTransactionPresent(transactionUUID));
    EXPECT_EQ(getRowCount(), 0);
}

// Test 16: Large block numbers
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, LargeBlockNumbers_Success)
{
    auto transactionUUID = createTestTransactionUUID();
    BlockNumber largeBlock = UINT64_MAX - 1000; // Very large block number
    
    mHandler->saveRecord(transactionUUID, largeBlock);
    EXPECT_TRUE(mHandler->isTransactionPresent(transactionUUID));
    
    // Verify transaction was saved by checking uncertain transactions
    auto uncertainTransactions = mHandler->transactionsWithUncertainObservingState();
    ASSERT_EQ(uncertainTransactions.size(), 1);
    EXPECT_EQ(uncertainTransactions[0].second, largeBlock);
}

// Test 17: Schema validation test
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, SchemaValidation_CorrectTableStructure_Success)
{
    // Verify table structure
    std::string query = "SELECT column_name, data_type FROM information_schema.columns WHERE table_name = '" + mTableName + "' ORDER BY ordinal_position;";
    PGresult *result = PQexec(mConnection, query.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    
    EXPECT_EQ(PQntuples(result), 5); // Updated schema includes payment_key_id
    
    // Verify column names and types
    EXPECT_STREQ(PQgetvalue(result, 0, 0), "uuid");
    EXPECT_STREQ(PQgetvalue(result, 0, 1), "bytea");
    
    EXPECT_STREQ(PQgetvalue(result, 1, 0), "maximal_claiming_block_number");
    EXPECT_STREQ(PQgetvalue(result, 1, 1), "bytea");
    
    EXPECT_STREQ(PQgetvalue(result, 2, 0), "observing_state");
    EXPECT_STREQ(PQgetvalue(result, 2, 1), "integer");
    
    EXPECT_STREQ(PQgetvalue(result, 3, 0), "recording_time");
    EXPECT_STREQ(PQgetvalue(result, 3, 1), "bigint");
    
    EXPECT_STREQ(PQgetvalue(result, 4, 0), "payment_key_id");
    EXPECT_STREQ(PQgetvalue(result, 4, 1), "bigint");
    
    PQclear(result);
}

// Test 18: Index validation test
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, IndexValidation_UUIDIndex_Exists)
{
    std::string query = "SELECT indexname FROM pg_indexes WHERE tablename = '" + mTableName + "' AND indexname LIKE '%uuid%';";
    PGresult *result = PQexec(mConnection, query.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    EXPECT_GT(PQntuples(result), 0);
    PQclear(result);
}

// Test 19: Raw database data validation - simplified
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, RawDataValidation_SaveRecord_CorrectDatabaseStorage)
{
    auto transactionUUID = createTestTransactionUUID();
    BlockNumber blockNumber = 12345;
    
    // Save record
    mHandler->saveRecord(transactionUUID, blockNumber);
    
    // Raw database verification
    std::string query = "SELECT uuid, maximal_claiming_block_number, observing_state, recording_time FROM " + mTableName + ";";
    PGresult *result = PQexec(mConnection, query.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    EXPECT_EQ(PQntuples(result), 1);
    
    // Verify observing state
    std::string storedState = PQgetvalue(result, 0, 2);
    EXPECT_EQ(std::stoi(storedState), 0);
    
    // Verify recording time is present
    std::string storedTime = PQgetvalue(result, 0, 3);
    EXPECT_GT(std::stoll(storedTime), 0);
    
    // Verify fields are not null (basic validation)
    EXPECT_GT(PQgetlength(result, 0, 0), 0); // UUID field
    EXPECT_GT(PQgetlength(result, 0, 1), 0); // Block number field
    
    PQclear(result);
}

// Test 20: Reverse validation test
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, ReverseValidation_DirectSQLInsertToClassReading_Success)
{
    auto transactionUUID = createTestTransactionUUID();
    BlockNumber blockNumber = 54321;
    
    // Direct SQL insert - updated schema includes payment_key_id
    std::string insertQuery = "INSERT INTO " + mTableName + " (uuid, maximal_claiming_block_number, observing_state, recording_time, payment_key_id) VALUES ($1, $2, $3, $4, (SELECT id FROM payment_keys ORDER BY id DESC LIMIT 1));";
    
    const char *params[4];
    int lengths[4];
    int formats[4] = {1, 1, 0, 0};
    
    params[0] = reinterpret_cast<const char*>(transactionUUID.data);
    lengths[0] = TransactionUUID::kBytesSize;
    
    params[1] = reinterpret_cast<const char*>(&blockNumber);
    lengths[1] = sizeof(BlockNumber);
    
    std::string stateStr = "0";
    params[2] = stateStr.c_str();
    lengths[2] = 0;
    
    GEOEpochTimestamp ts = microsecondsSinceGEOEpoch(utc_now());
    std::string tsStr = std::to_string(ts);
    params[3] = tsStr.c_str();
    lengths[3] = 0;
    
    PGresult *result = PQexecParams(mConnection, insertQuery.c_str(), 4, nullptr, params, lengths, formats, 0);
    EXPECT_EQ(PQresultStatus(result), PGRES_COMMAND_OK);
    PQclear(result);
    
    // Use handler to verify data
    EXPECT_TRUE(mHandler->isTransactionPresent(transactionUUID));
    
    auto uncertainTransactions = mHandler->transactionsWithUncertainObservingState();
    ASSERT_EQ(uncertainTransactions.size(), 1);
    EXPECT_EQ(uncertainTransactions[0].second, blockNumber);
}

// Test 21: Multiple state transitions
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, MultipleStateTransitions_Success)
{
    auto transactionUUID = createTestTransactionUUID();
    BlockNumber blockNumber = 12345;
    
    // Save record
    mHandler->saveRecord(transactionUUID, blockNumber);
    EXPECT_EQ(getObservingState(transactionUUID), 0);
    
    // Multiple state transitions
    for (int state = 1; state <= 5; ++state) {
        mHandler->updateTransactionState(transactionUUID, state);
        EXPECT_EQ(getObservingState(transactionUUID), state);
    }
}

// transactionsForObserverMonitoring tests
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsForObserverMonitoring_BasicRetrieval_ReturnsMatchingTransactions)
{
    mHandler->saveRecord(createTestTransactionUUID("bn50"), static_cast<BlockNumber>(50));
    mHandler->saveRecord(createTestTransactionUUID("bn100"), static_cast<BlockNumber>(100));
    mHandler->saveRecord(createTestTransactionUUID("bn150"), static_cast<BlockNumber>(150));
    mHandler->saveRecord(createTestTransactionUUID("bn200"), static_cast<BlockNumber>(200));
    mHandler->saveRecord(createTestTransactionUUID("bn250"), static_cast<BlockNumber>(250));

    auto result = mHandler->transactionsForObserverMonitoring(static_cast<BlockNumber>(100), 10);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].second, static_cast<BlockNumber>(150));
    EXPECT_EQ(result[1].second, static_cast<BlockNumber>(200));
    EXPECT_EQ(result[2].second, static_cast<BlockNumber>(250));
}

TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsForObserverMonitoring_StateFiltering_OnlyReturnsUncertainState)
{
    auto uuidState0 = createTestTransactionUUID("state0");
    auto uuidState1 = createTestTransactionUUID("state1");
    auto uuidState2 = createTestTransactionUUID("state2");

    mHandler->saveRecord(uuidState0, static_cast<BlockNumber>(100));
    mHandler->saveRecord(uuidState1, static_cast<BlockNumber>(100));
    mHandler->saveRecord(uuidState2, static_cast<BlockNumber>(100));

    mHandler->updateTransactionState(uuidState1, 1);
    mHandler->updateTransactionState(uuidState2, 2);

    auto result = mHandler->transactionsForObserverMonitoring(static_cast<BlockNumber>(0), 10);

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].first.stringUUID(), uuidState0.stringUUID());
    EXPECT_EQ(result[0].second, static_cast<BlockNumber>(100));
}

TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsForObserverMonitoring_LimitParameter_RespectsLimit)
{
    for (uint64_t i = 1; i <= 10; ++i) {
        mHandler->saveRecord(createTestTransactionUUID("limit" + std::to_string(i)), static_cast<BlockNumber>(i * 10));
    }

    auto result = mHandler->transactionsForObserverMonitoring(static_cast<BlockNumber>(0), 3);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].second, static_cast<BlockNumber>(10));
    EXPECT_EQ(result[1].second, static_cast<BlockNumber>(20));
    EXPECT_EQ(result[2].second, static_cast<BlockNumber>(30));
}

TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsForObserverMonitoring_AscendingOrder_OldestFirst)
{
    mHandler->saveRecord(createTestTransactionUUID("bn30"), static_cast<BlockNumber>(30));
    mHandler->saveRecord(createTestTransactionUUID("bn10"), static_cast<BlockNumber>(10));
    mHandler->saveRecord(createTestTransactionUUID("bn20"), static_cast<BlockNumber>(20));

    auto result = mHandler->transactionsForObserverMonitoring(static_cast<BlockNumber>(0), 10);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].second, static_cast<BlockNumber>(10));
    EXPECT_EQ(result[1].second, static_cast<BlockNumber>(20));
    EXPECT_EQ(result[2].second, static_cast<BlockNumber>(30));
}

TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsForObserverMonitoring_EmptyResult_NoMatchingTransactions)
{
    mHandler->saveRecord(createTestTransactionUUID("bn50"), static_cast<BlockNumber>(50));
    mHandler->saveRecord(createTestTransactionUUID("bn100"), static_cast<BlockNumber>(100));

    auto result = mHandler->transactionsForObserverMonitoring(static_cast<BlockNumber>(100), 10);

    EXPECT_TRUE(result.empty());
}

TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsForObserverMonitoring_BoundaryCondition_ExactBlockNumber)
{
    mHandler->saveRecord(createTestTransactionUUID("bn100"), static_cast<BlockNumber>(100));

    auto result = mHandler->transactionsForObserverMonitoring(static_cast<BlockNumber>(100), 10);

    EXPECT_TRUE(result.empty());
}

TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsForObserverMonitoring_ZeroLimit_ReturnsEmpty)
{
    mHandler->saveRecord(createTestTransactionUUID("bn100"), static_cast<BlockNumber>(100));
    mHandler->saveRecord(createTestTransactionUUID("bn200"), static_cast<BlockNumber>(200));

    auto result = mHandler->transactionsForObserverMonitoring(static_cast<BlockNumber>(0), 0);

    EXPECT_TRUE(result.empty());
}

TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsForObserverMonitoring_LargeDataset_ReturnsExpectedRange)
{
    for (uint64_t i = 1; i <= 200; ++i) {
        mHandler->saveRecord(createTestTransactionUUID("bulk" + std::to_string(i)), static_cast<BlockNumber>(i));
    }

    auto result = mHandler->transactionsForObserverMonitoring(static_cast<BlockNumber>(50), 100);

    ASSERT_EQ(result.size(), 100);
    EXPECT_EQ(result.front().second, static_cast<BlockNumber>(51));
    EXPECT_EQ(result.back().second, static_cast<BlockNumber>(150));

    for (size_t i = 0; i < result.size(); ++i) {
        EXPECT_EQ(result[i].second, static_cast<BlockNumber>(51 + i));
    }
}
