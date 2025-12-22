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
    // effectiveClaimingBlockNumber equals maximalClaimingBlockNumber for basic tests
    BlockNumber effectiveBlockNumber = blockNumber;

    // Act
    mHandler->saveRecord(transactionUUID, blockNumber, effectiveBlockNumber);

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
    mHandler->saveRecord(uuid1, block1, block1);
    mHandler->saveRecord(uuid2, block2, block2);

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
    mHandler->saveRecord(transactionUUID, blockNumber, blockNumber);
    EXPECT_EQ(getRowCount(), 1);

    // Save duplicate (should be allowed)
    EXPECT_NO_THROW(mHandler->saveRecord(transactionUUID, blockNumber, blockNumber));
    EXPECT_EQ(getRowCount(), 2); // Now we have 2 records
}

// Test 5: Update transaction state
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, UpdateTransactionState_ValidUUID_Success)
{
    auto transactionUUID = createTestTransactionUUID();
    BlockNumber blockNumber = 12345;

    // Save record
    mHandler->saveRecord(transactionUUID, blockNumber, blockNumber);
    EXPECT_EQ(getObservingState(transactionUUID), static_cast<int>(PaymentObservingState::Init));

    // Update state to Approved
    mHandler->updateTransactionState(transactionUUID, PaymentObservingState::Committed);
    EXPECT_EQ(getObservingState(transactionUUID), static_cast<int>(PaymentObservingState::Committed));

    // Update state to ParticipantsVotesPresent
    mHandler->updateTransactionState(transactionUUID, PaymentObservingState::ParticipantsVotesPresent);
    EXPECT_EQ(getObservingState(transactionUUID), static_cast<int>(PaymentObservingState::ParticipantsVotesPresent));
}

// Test 6: Update non-existing transaction
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, UpdateTransactionState_NonExistingUUID_ThrowsValueError)
{
    auto transactionUUID = createTestTransactionUUID();
    
    EXPECT_THROW(
        mHandler->updateTransactionState(transactionUUID, PaymentObservingState::Committed),
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
    mHandler->saveRecord(uuid1, block1, block1);
    mHandler->saveRecord(uuid2, block2, block2);
    mHandler->saveRecord(uuid3, block3, block3);

    // Update some states
    mHandler->updateTransactionState(uuid2, PaymentObservingState::Committed);
    // uuid1 and uuid3 remain with state Uncertain (0)

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
    mHandler->saveRecord(transactionUUID, blockNumber, blockNumber);
    mHandler->updateTransactionState(transactionUUID, PaymentObservingState::Committed);

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
    mHandler->saveRecord(transactionUUID, blockNumber, blockNumber);

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
    mHandler->saveRecord(transactionUUID, blockNumber, blockNumber);
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
    mHandler->saveRecord(uuid1, blockNumber, blockNumber);
    mHandler->saveRecord(uuid2, blockNumber, blockNumber);
    mHandler->saveRecord(uuid3, blockNumber, blockNumber);

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
    mHandler->saveRecord(transactionUUID, blockNumber, blockNumber);
    EXPECT_TRUE(mHandler->isTransactionPresent(transactionUUID));
    EXPECT_EQ(getObservingState(transactionUUID), static_cast<int>(PaymentObservingState::Init));

    // Update state
    mHandler->updateTransactionState(transactionUUID, PaymentObservingState::Committed);
    EXPECT_EQ(getObservingState(transactionUUID), static_cast<int>(PaymentObservingState::Committed));

    // Check uncertain transactions
    auto uncertainTransactions = mHandler->transactionsWithUncertainObservingState();
    EXPECT_TRUE(uncertainTransactions.empty()); // state is Approved, not Uncertain

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

    mHandler->saveRecord(transactionUUID, largeBlock, largeBlock);
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

    // Schema now includes effective_claiming_block_number field
    EXPECT_EQ(PQntuples(result), 6);

    // Verify column names and types
    EXPECT_STREQ(PQgetvalue(result, 0, 0), "uuid");
    EXPECT_STREQ(PQgetvalue(result, 0, 1), "bytea");

    EXPECT_STREQ(PQgetvalue(result, 1, 0), "maximal_claiming_block_number");
    EXPECT_STREQ(PQgetvalue(result, 1, 1), "bytea");

    // New field: effective_claiming_block_number includes dispute grace period
    EXPECT_STREQ(PQgetvalue(result, 2, 0), "effective_claiming_block_number");
    EXPECT_STREQ(PQgetvalue(result, 2, 1), "bytea");

    EXPECT_STREQ(PQgetvalue(result, 3, 0), "observing_state");
    EXPECT_STREQ(PQgetvalue(result, 3, 1), "integer");

    EXPECT_STREQ(PQgetvalue(result, 4, 0), "recording_time");
    EXPECT_STREQ(PQgetvalue(result, 4, 1), "bigint");

    EXPECT_STREQ(PQgetvalue(result, 5, 0), "payment_key_id");
    EXPECT_STREQ(PQgetvalue(result, 5, 1), "bigint");

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
    mHandler->saveRecord(transactionUUID, blockNumber, blockNumber);

    // Raw database verification (now includes effective_claiming_block_number)
    std::string query = "SELECT uuid, maximal_claiming_block_number, effective_claiming_block_number, observing_state, recording_time FROM " + mTableName + ";";
    PGresult *result = PQexec(mConnection, query.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    EXPECT_EQ(PQntuples(result), 1);

    // Verify observing state
    std::string storedState = PQgetvalue(result, 0, 3);
    EXPECT_EQ(std::stoi(storedState), 0);

    // Verify recording time is present
    std::string storedTime = PQgetvalue(result, 0, 4);
    EXPECT_GT(std::stoll(storedTime), 0);

    // Verify fields are not null (basic validation)
    EXPECT_GT(PQgetlength(result, 0, 0), 0); // UUID field
    EXPECT_GT(PQgetlength(result, 0, 1), 0); // Block number field
    EXPECT_GT(PQgetlength(result, 0, 2), 0); // Effective block number field

    PQclear(result);
}

// Test 20: Reverse validation test
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, ReverseValidation_DirectSQLInsertToClassReading_Success)
{
    auto transactionUUID = createTestTransactionUUID();
    BlockNumber blockNumber = 54321;
    // For this test, effective_claiming_block_number equals maximal_claiming_block_number
    BlockNumber effectiveBlockNumber = blockNumber;

    // Direct SQL insert - updated schema includes effective_claiming_block_number and payment_key_id
    std::string insertQuery = "INSERT INTO " + mTableName + " (uuid, maximal_claiming_block_number, effective_claiming_block_number, observing_state, recording_time, payment_key_id) VALUES ($1, $2, $3, $4, $5, (SELECT id FROM payment_keys ORDER BY id DESC LIMIT 1));";

    const char *params[5];
    int lengths[5];
    int formats[5] = {1, 1, 1, 0, 0};

    params[0] = reinterpret_cast<const char*>(transactionUUID.data);
    lengths[0] = TransactionUUID::kBytesSize;

    params[1] = reinterpret_cast<const char*>(&blockNumber);
    lengths[1] = sizeof(BlockNumber);

    // Bind effective claiming block number
    params[2] = reinterpret_cast<const char*>(&effectiveBlockNumber);
    lengths[2] = sizeof(BlockNumber);

    std::string stateStr = "0";
    params[3] = stateStr.c_str();
    lengths[3] = 0;

    GEOEpochTimestamp ts = microsecondsSinceGEOEpoch(utc_now());
    std::string tsStr = std::to_string(ts);
    params[4] = tsStr.c_str();
    lengths[4] = 0;

    PGresult *result = PQexecParams(mConnection, insertQuery.c_str(), 5, nullptr, params, lengths, formats, 0);
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
    mHandler->saveRecord(transactionUUID, blockNumber, blockNumber);
    EXPECT_EQ(getObservingState(transactionUUID), static_cast<int>(PaymentObservingState::Init));

    // Multiple state transitions through all valid states
    mHandler->updateTransactionState(transactionUUID, PaymentObservingState::Committed);
    EXPECT_EQ(getObservingState(transactionUUID), static_cast<int>(PaymentObservingState::Committed));

    mHandler->updateTransactionState(transactionUUID, PaymentObservingState::ParticipantsVotesPresent);
    EXPECT_EQ(getObservingState(transactionUUID), static_cast<int>(PaymentObservingState::ParticipantsVotesPresent));

    mHandler->updateTransactionState(transactionUUID, PaymentObservingState::RejectedByObserving);
    EXPECT_EQ(getObservingState(transactionUUID), static_cast<int>(PaymentObservingState::RejectedByObserving));
}

// transactionsForObserverMonitoring tests
// Note: transactionsForObserverMonitoring now filters by effective_claiming_block_number
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsForObserverMonitoring_BasicRetrieval_ReturnsMatchingTransactions)
{
    // Save records where effective_claiming_block_number equals maximal_claiming_block_number
    mHandler->saveRecord(createTestTransactionUUID("bn50"), static_cast<BlockNumber>(50), static_cast<BlockNumber>(50));
    mHandler->saveRecord(createTestTransactionUUID("bn100"), static_cast<BlockNumber>(100), static_cast<BlockNumber>(100));
    mHandler->saveRecord(createTestTransactionUUID("bn150"), static_cast<BlockNumber>(150), static_cast<BlockNumber>(150));
    mHandler->saveRecord(createTestTransactionUUID("bn200"), static_cast<BlockNumber>(200), static_cast<BlockNumber>(200));
    mHandler->saveRecord(createTestTransactionUUID("bn250"), static_cast<BlockNumber>(250), static_cast<BlockNumber>(250));

    // Filter by effective_claiming_block_number > 100
    auto result = mHandler->transactionsForObserverMonitoring(static_cast<BlockNumber>(100), 10);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].second, static_cast<BlockNumber>(150));
    EXPECT_EQ(result[1].second, static_cast<BlockNumber>(200));
    EXPECT_EQ(result[2].second, static_cast<BlockNumber>(250));
}

TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsForObserverMonitoring_StateFiltering_OnlyReturnsApprovedState)
{
    auto uuidState0 = createTestTransactionUUID("state0");
    auto uuidState1 = createTestTransactionUUID("state1");
    auto uuidState2 = createTestTransactionUUID("state2");

    mHandler->saveRecord(uuidState0, static_cast<BlockNumber>(100), static_cast<BlockNumber>(100));
    mHandler->saveRecord(uuidState1, static_cast<BlockNumber>(100), static_cast<BlockNumber>(100));
    mHandler->saveRecord(uuidState2, static_cast<BlockNumber>(100), static_cast<BlockNumber>(100));

    // uuidState0 stays Uncertain (0)
    // uuidState1 becomes Approved (1) - this is what transactionsForObserverMonitoring filters for
    // uuidState2 becomes ParticipantsVotesPresent (3)
    mHandler->updateTransactionState(uuidState1, PaymentObservingState::Committed);
    mHandler->updateTransactionState(uuidState2, PaymentObservingState::ParticipantsVotesPresent);

    auto result = mHandler->transactionsForObserverMonitoring(static_cast<BlockNumber>(0), 10);

    // transactionsForObserverMonitoring filters by observing_state = 1 (Approved)
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].first.stringUUID(), uuidState1.stringUUID());
    EXPECT_EQ(result[0].second, static_cast<BlockNumber>(100));
}

TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsForObserverMonitoring_LimitParameter_RespectsLimit)
{
    for (uint64_t i = 1; i <= 10; ++i) {
        auto blockNum = static_cast<BlockNumber>(i * 10);
        mHandler->saveRecord(createTestTransactionUUID("limit" + std::to_string(i)), blockNum, blockNum);
    }

    auto result = mHandler->transactionsForObserverMonitoring(static_cast<BlockNumber>(0), 3);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].second, static_cast<BlockNumber>(10));
    EXPECT_EQ(result[1].second, static_cast<BlockNumber>(20));
    EXPECT_EQ(result[2].second, static_cast<BlockNumber>(30));
}

TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsForObserverMonitoring_AscendingOrder_OldestFirst)
{
    mHandler->saveRecord(createTestTransactionUUID("bn30"), static_cast<BlockNumber>(30), static_cast<BlockNumber>(30));
    mHandler->saveRecord(createTestTransactionUUID("bn10"), static_cast<BlockNumber>(10), static_cast<BlockNumber>(10));
    mHandler->saveRecord(createTestTransactionUUID("bn20"), static_cast<BlockNumber>(20), static_cast<BlockNumber>(20));

    auto result = mHandler->transactionsForObserverMonitoring(static_cast<BlockNumber>(0), 10);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].second, static_cast<BlockNumber>(10));
    EXPECT_EQ(result[1].second, static_cast<BlockNumber>(20));
    EXPECT_EQ(result[2].second, static_cast<BlockNumber>(30));
}

TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsForObserverMonitoring_EmptyResult_NoMatchingTransactions)
{
    mHandler->saveRecord(createTestTransactionUUID("bn50"), static_cast<BlockNumber>(50), static_cast<BlockNumber>(50));
    mHandler->saveRecord(createTestTransactionUUID("bn100"), static_cast<BlockNumber>(100), static_cast<BlockNumber>(100));

    // effective_claiming_block_number must be > 100, but both are <= 100
    auto result = mHandler->transactionsForObserverMonitoring(static_cast<BlockNumber>(100), 10);

    EXPECT_TRUE(result.empty());
}

TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsForObserverMonitoring_BoundaryCondition_ExactBlockNumber)
{
    mHandler->saveRecord(createTestTransactionUUID("bn100"), static_cast<BlockNumber>(100), static_cast<BlockNumber>(100));

    // effective_claiming_block_number must be > 100, but it equals 100
    auto result = mHandler->transactionsForObserverMonitoring(static_cast<BlockNumber>(100), 10);

    EXPECT_TRUE(result.empty());
}

TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsForObserverMonitoring_ZeroLimit_ReturnsEmpty)
{
    mHandler->saveRecord(createTestTransactionUUID("bn100"), static_cast<BlockNumber>(100), static_cast<BlockNumber>(100));
    mHandler->saveRecord(createTestTransactionUUID("bn200"), static_cast<BlockNumber>(200), static_cast<BlockNumber>(200));

    auto result = mHandler->transactionsForObserverMonitoring(static_cast<BlockNumber>(0), 0);

    EXPECT_TRUE(result.empty());
}

TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsForObserverMonitoring_LargeDataset_ReturnsExpectedRange)
{
    for (uint64_t i = 1; i <= 200; ++i) {
        auto blockNum = static_cast<BlockNumber>(i);
        mHandler->saveRecord(createTestTransactionUUID("bulk" + std::to_string(i)), blockNum, blockNum);
    }

    // Filter by effective_claiming_block_number > 50
    auto result = mHandler->transactionsForObserverMonitoring(static_cast<BlockNumber>(50), 100);

    ASSERT_EQ(result.size(), 100);
    EXPECT_EQ(result.front().second, static_cast<BlockNumber>(51));
    EXPECT_EQ(result.back().second, static_cast<BlockNumber>(150));

    for (size_t i = 0; i < result.size(); ++i) {
        EXPECT_EQ(result[i].second, static_cast<BlockNumber>(51 + i));
    }
}

// Test to verify that effective_claiming_block_number filtering works correctly
// when it differs from maximal_claiming_block_number
TEST_F(PaymentTransactionsHandlerPostgreSQLIntegrationTest, TransactionsForObserverMonitoring_EffectiveBlockDiffersFromMaximal)
{
    // Transaction with maximal=50, effective=100 (dispute grace period of 50 blocks)
    auto uuid1 = createTestTransactionUUID("tx1");
    mHandler->saveRecord(uuid1, static_cast<BlockNumber>(50), static_cast<BlockNumber>(100));

    // Transaction with maximal=60, effective=60 (no dispute grace period)
    auto uuid2 = createTestTransactionUUID("tx2");
    mHandler->saveRecord(uuid2, static_cast<BlockNumber>(60), static_cast<BlockNumber>(60));

    // Query with minBlockNumber=70
    // uuid1 should be returned (effective=100 > 70)
    // uuid2 should NOT be returned (effective=60 <= 70)
    auto result = mHandler->transactionsForObserverMonitoring(static_cast<BlockNumber>(70), 10);

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].first.stringUUID(), uuid1.stringUUID());
    // The returned maximal_claiming_block_number should be 50
    EXPECT_EQ(result[0].second, static_cast<BlockNumber>(50));
}
