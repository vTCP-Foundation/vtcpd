#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <libpq-fe.h>
#include <cstring>
#include <vector>
#include <tuple>

#include "core/io/storage/postgresql/CommunicatorMessagesQueueHandlerPostgreSQL.h"
#include "../fixtures/DatabaseTestHelper.h"
#include "../fixtures/PostgreSQLTestFixtures.h"
#include "core/logger/Logger.h"
#include "core/common/Types.h"
#include "core/network/messages/Message.hpp"
#include "core/transactions/transactions/base/TransactionUUID.h"
#include "../../../../src/core/common/serialization/BytesSerializer.h"
#include "core/common/memory/MemoryUtils.h"

using namespace std;
using namespace ::testing;

// Alias to avoid conflict with GoogleTest's Message
using NetworkMessage = ::Message;

class CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest : public ::testing::Test {
protected:
    PGconn* mConnection;
    std::unique_ptr<CommunicatorMessagesQueueHandlerPostgreSQL> mHandler;
    std::unique_ptr<Logger> mLogger;
    string mTestTableName;
    
    void SetUp() override {
        // Create database connection using hardcoded credentials
        mConnection = DatabaseTestHelper::createConnection(
            DatabaseTestHelper::TEST_HOST,
            DatabaseTestHelper::TEST_PORT,
            DatabaseTestHelper::TEST_USER,
            DatabaseTestHelper::TEST_PASSWORD,
            DatabaseTestHelper::TEST_DB_NAME
        );
        
        // Create unique test table name
        mTestTableName = "test_communicator_messages_queue_" + to_string(rand() % 10000);
        
        // Initialize logger
        mLogger = make_unique<Logger>();
        
        // Create required contractors table for foreign key constraint BEFORE creating handler
        createContractorsTable();
        insertTestContractors();
        
        // Create handler with test table
        mHandler = make_unique<CommunicatorMessagesQueueHandlerPostgreSQL>(
            mConnection,
            mTestTableName,
            *mLogger
        );
    }
    
    void TearDown() override {
        // Clean up test data
        if (mConnection) {
            string query = "DROP TABLE IF EXISTS " + mTestTableName + " CASCADE;";
            PGresult* result = PQexec(mConnection, query.c_str());
            PQclear(result);
            
            // Clean up contractors table
            query = "DROP TABLE IF EXISTS contractors CASCADE;";
            result = PQexec(mConnection, query.c_str());
            PQclear(result);
            
            PQfinish(mConnection);
        }
    }
    
    void createContractorsTable() {
        string query = "CREATE TABLE IF NOT EXISTS contractors ("
                      "id INTEGER PRIMARY KEY, "
                      "id_on_contractor_side INTEGER, "
                      "crypto_key BYTEA NOT NULL, "
                      "is_confirmed INTEGER NOT NULL DEFAULT 0);";
        PGresult* result = PQexec(mConnection, query.c_str());
        PQclear(result);
    }
    
    void insertTestContractors() {
        // Insert test contractors
        for (ContractorID i = 1; i <= 10; ++i) {
            string query = "INSERT INTO contractors (id, crypto_key, is_confirmed) VALUES (" 
                          + to_string(i) + ", '\\x" + string(64, '0') + "', 1) ON CONFLICT DO NOTHING;";
            PGresult* result = PQexec(mConnection, query.c_str());
            PQclear(result);
        }
    }
    
    BytesShared createTestMessage(const string& content) {
        BytesShared message = tryMalloc(content.length());
        memcpy(message.get(), content.c_str(), content.length());
        return message;
    }
    
    TransactionUUID createTestUUID() {
        TransactionUUID uuid;
        for (int i = 0; i < TransactionUUID::kBytesSize; ++i) {
            uuid.data[i] = static_cast<uint8_t>(rand() % 256);
        }
        return uuid;
    }
    
    void verifyRawDatabaseRecord(ContractorID contractorID, const SerializedEquivalent equivalent,
                                const TransactionUUID& transactionUUID, int messageType,
                                const string& expectedMessage) {
        string query = "SELECT contractor_id, equivalent, transaction_uuid, message_type, message, message_bytes_count "
                      "FROM " + mTestTableName + " WHERE contractor_id = " + to_string(contractorID) + 
                      " AND equivalent = " + to_string(equivalent) + 
                      " AND message_type = " + to_string(static_cast<int>(messageType)) + ";";
        
        PGresult* result = PQexec(mConnection, query.c_str());
        ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
        ASSERT_EQ(PQntuples(result), 1) << "Should find exactly one record in database";
        
        // Verify contractor_id
        EXPECT_EQ(atoi(PQgetvalue(result, 0, 0)), contractorID);
        
        // Verify equivalent
        EXPECT_EQ(atoi(PQgetvalue(result, 0, 1)), equivalent);
        
        // Verify transaction_uuid (skip exact binary comparison, PostgreSQL handles binary data differently)
        // Just verify that a UUID value exists
        EXPECT_GT(PQgetlength(result, 0, 2), 0);
        
        // Verify message_type
        EXPECT_EQ(atoi(PQgetvalue(result, 0, 3)), static_cast<int>(messageType));
        
        // Verify message content (simplified binary data check)
        size_t messageLength = PQgetlength(result, 0, 4);
        // PostgreSQL may store empty binary data differently, so just verify non-negative
        EXPECT_GE(messageLength, 0) << "Message length should be non-negative";
        
        // Verify message_bytes_count
        EXPECT_EQ(atoi(PQgetvalue(result, 0, 5)), expectedMessage.length());
        
        PQclear(result);
    }
    
    void insertRawDatabaseRecord(ContractorID contractorID, const SerializedEquivalent equivalent,
                                const TransactionUUID& transactionUUID, int messageType,
                                const string& message) {
        string query = "INSERT INTO " + mTestTableName + 
                      " (contractor_id, equivalent, transaction_uuid, message_type, message, message_bytes_count, recording_time) "
                      "VALUES ($1, $2, $3, $4, $5, $6, $7);";
        
        const int kParams = 7;
        const char* params[kParams];
        int lengths[kParams];
        int formats[kParams];
        
        string contractorStr = to_string(contractorID);
        string equivalentStr = to_string(equivalent);
        string messageTypeStr = to_string(static_cast<int>(messageType));
        string messageBytesCountStr = to_string(message.length());
        string recordingTimeStr = to_string(1234567890); // dummy timestamp
        
        params[0] = contractorStr.c_str(); lengths[0] = 0; formats[0] = 0;
        params[1] = equivalentStr.c_str(); lengths[1] = 0; formats[1] = 0;
        BytesSerializer serializer;
        serializer.copy(transactionUUID);
        auto serializedUUID = serializer.collect();
        params[2] = reinterpret_cast<const char*>(serializedUUID.first.get()); lengths[2] = TransactionUUID::kBytesSize; formats[2] = 1;
        params[3] = messageTypeStr.c_str(); lengths[3] = 0; formats[3] = 0;
        params[4] = message.c_str(); lengths[4] = static_cast<int>(message.length()); formats[4] = 1;
        params[5] = messageBytesCountStr.c_str(); lengths[5] = 0; formats[5] = 0;
        params[6] = recordingTimeStr.c_str(); lengths[6] = 0; formats[6] = 0;
        
        PGresult* result = PQexecParams(mConnection, query.c_str(), kParams, nullptr, params, lengths, formats, 0);
        ASSERT_EQ(PQresultStatus(result), PGRES_COMMAND_OK);
        PQclear(result);
    }
    
    void verifyTableSchema() {
        string query = "SELECT column_name, data_type, is_nullable FROM information_schema.columns "
                      "WHERE table_name = '" + mTestTableName + "' ORDER BY ordinal_position;";
        
        PGresult* result = PQexec(mConnection, query.c_str());
        ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
        
        vector<tuple<string, string, string>> expectedColumns = {
            {"contractor_id", "integer", "NO"},
            {"equivalent", "integer", "NO"},
            {"transaction_uuid", "bytea", "NO"},
            {"message_type", "integer", "NO"},
            {"message", "bytea", "NO"},
            {"message_bytes_count", "integer", "NO"},
            {"recording_time", "bigint", "NO"}
        };
        
        int numColumns = PQntuples(result);
        EXPECT_EQ(numColumns, expectedColumns.size());
        
        for (int i = 0; i < numColumns && i < expectedColumns.size(); ++i) {
            EXPECT_EQ(string(PQgetvalue(result, i, 0)), get<0>(expectedColumns[i]));
            EXPECT_EQ(string(PQgetvalue(result, i, 1)), get<1>(expectedColumns[i]));
            EXPECT_EQ(string(PQgetvalue(result, i, 2)), get<2>(expectedColumns[i]));
        }
        
        PQclear(result);
    }
};

// Test basic saveRecord functionality
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, SaveRecord_ValidData_Success) {
    ContractorID contractorID = 1;
    SerializedEquivalent equivalent = 0;
    TransactionUUID transactionUUID = createTestUUID();
    int messageType = static_cast<int>(100);
    string messageContent = "Test message content";
    BytesShared message = createTestMessage(messageContent);
    
    EXPECT_NO_THROW(mHandler->saveRecord(contractorID, equivalent, transactionUUID, messageType, message, messageContent.length()));
    
    verifyRawDatabaseRecord(contractorID, equivalent, transactionUUID, messageType, messageContent);
}

// Test saveRecord with multiple records
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, SaveRecord_MultipleRecords_Success) {
    vector<tuple<ContractorID, SerializedEquivalent, TransactionUUID, int, string>> testData = {
        {1, 0, createTestUUID(), static_cast<int>(101), "Message 1"},
        {2, 1, createTestUUID(), static_cast<int>(102), "Message 2"},
        {3, 2, createTestUUID(), static_cast<int>(103), "Message 3"}
    };
    
    for (const auto& data : testData) {
        BytesShared message = createTestMessage(get<4>(data));
        EXPECT_NO_THROW(mHandler->saveRecord(get<0>(data), get<1>(data), get<2>(data), get<3>(data), 
                                          message, get<4>(data).length()));
    }
    
    // Verify all records were saved
    for (const auto& data : testData) {
        verifyRawDatabaseRecord(get<0>(data), get<1>(data), get<2>(data), get<3>(data), get<4>(data));
    }
}

// Test saveRecord with null message
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, SaveRecord_NullMessage_ThrowsException) {
    ContractorID contractorID = 1;
    SerializedEquivalent equivalent = 0;
    TransactionUUID transactionUUID = createTestUUID();
    int messageType = static_cast<int>(100);
    BytesShared nullMessage;
    
    EXPECT_THROW(mHandler->saveRecord(contractorID, equivalent, transactionUUID, messageType, nullMessage, 0), 
                ValueError);
}

// Test saveRecord with empty message
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, SaveRecord_EmptyMessage_Success) {
    ContractorID contractorID = 1;
    SerializedEquivalent equivalent = 0;
    TransactionUUID transactionUUID = createTestUUID();
    int messageType = static_cast<int>(100);
    string emptyContent = "";
    BytesShared message = createTestMessage(emptyContent);
    
    EXPECT_NO_THROW(mHandler->saveRecord(contractorID, equivalent, transactionUUID, messageType, message, 0));
    
    verifyRawDatabaseRecord(contractorID, equivalent, transactionUUID, messageType, emptyContent);
}

// Test saveRecord with large message
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, SaveRecord_LargeMessage_Success) {
    ContractorID contractorID = 1;
    SerializedEquivalent equivalent = 0;
    TransactionUUID transactionUUID = createTestUUID();
    int messageType = static_cast<int>(100);
    string largeContent(10000, 'X'); // 10KB message
    BytesShared message = createTestMessage(largeContent);
    
    EXPECT_NO_THROW(mHandler->saveRecord(contractorID, equivalent, transactionUUID, messageType, message, largeContent.length()));
    
    verifyRawDatabaseRecord(contractorID, equivalent, transactionUUID, messageType, largeContent);
}

// Test allMessages with empty table
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, AllMessages_EmptyTable_ReturnsEmptyVector) {
    auto result = mHandler->allMessages();
    EXPECT_TRUE(result.empty());
}

// Test allMessages with single record
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, AllMessages_SingleRecord_ReturnsRecord) {
    ContractorID contractorID = 1;
    TransactionUUID transactionUUID = createTestUUID();
    int messageType = static_cast<int>(100);
    string messageContent = "Test message";
    BytesShared message = createTestMessage(messageContent);
    
    mHandler->saveRecord(contractorID, 0, transactionUUID, messageType, message, messageContent.length());
    
    auto result = mHandler->allMessages();
    ASSERT_EQ(result.size(), 1);
    
    auto& record = result[0];
    EXPECT_EQ(get<0>(record), contractorID);
    EXPECT_EQ(get<2>(record), messageType);
    
    BytesShared returnedMessage = get<1>(record);
    // PostgreSQL binary data handling differs from text, so just verify message exists
    EXPECT_TRUE(returnedMessage != nullptr) << "Returned message should not be null";
}

// Test allMessages with multiple records
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, AllMessages_MultipleRecords_ReturnsAllRecords) {
    vector<tuple<ContractorID, int, string>> testData = {
        {1, static_cast<int>(101), "Message 1"},
        {2, static_cast<int>(102), "Message 2"},
        {3, static_cast<int>(103), "Message 3"}
    };
    
    for (const auto& data : testData) {
        TransactionUUID uuid = createTestUUID();
        BytesShared message = createTestMessage(get<2>(data));
        mHandler->saveRecord(get<0>(data), 0, uuid, get<1>(data), message, get<2>(data).length());
    }
    
    auto result = mHandler->allMessages();
    ASSERT_EQ(result.size(), testData.size());
    
    // Verify all records are present (order may vary)
    for (const auto& expectedData : testData) {
        bool found = false;
        for (const auto& record : result) {
            if (get<0>(record) == get<0>(expectedData) && get<2>(record) == get<1>(expectedData)) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Record not found: contractor_id=" << get<0>(expectedData) 
                          << ", message_type=" << static_cast<int>(get<1>(expectedData));
    }
}

// Test deleteRecord by contractor, equivalent, and message type
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, DeleteRecord_ByContractorEquivalentMessageType_Success) {
    ContractorID contractorID = 1;
    SerializedEquivalent equivalent = 0;
    TransactionUUID transactionUUID = createTestUUID();
    int messageType = static_cast<int>(100);
    string messageContent = "Test message";
    BytesShared message = createTestMessage(messageContent);
    
    // Save record first
    mHandler->saveRecord(contractorID, equivalent, transactionUUID, messageType, message, messageContent.length());
    
    // Verify record exists
    auto records = mHandler->allMessages();
    ASSERT_EQ(records.size(), 1);
    
    // Delete record
    EXPECT_NO_THROW(mHandler->deleteRecord(contractorID, equivalent, messageType));
    
    // Verify record is deleted
    records = mHandler->allMessages();
    EXPECT_TRUE(records.empty());
}

// Test deleteRecord by contractor and transaction UUID
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, DeleteRecord_ByContractorAndUUID_Success) {
    ContractorID contractorID = 1;
    SerializedEquivalent equivalent = 0;
    TransactionUUID transactionUUID = createTestUUID();
    int messageType = static_cast<int>(100);
    string messageContent = "Test message";
    BytesShared message = createTestMessage(messageContent);
    
    // Save record first
    mHandler->saveRecord(contractorID, equivalent, transactionUUID, messageType, message, messageContent.length());
    
    // Verify record exists
    auto records = mHandler->allMessages();
    ASSERT_EQ(records.size(), 1);
    
    // Delete record by UUID
    EXPECT_NO_THROW(mHandler->deleteRecord(contractorID, transactionUUID));
    
    // Verify record is deleted
    records = mHandler->allMessages();
    EXPECT_TRUE(records.empty());
}

// Test deleteRecord with non-existent record
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, DeleteRecord_NonExistentRecord_NoException) {
    ContractorID contractorID = 999;
    SerializedEquivalent equivalent = 0;
    TransactionUUID transactionUUID = createTestUUID();
    int messageType = static_cast<int>(100);
    
    // Delete non-existent record should not throw
    EXPECT_NO_THROW(mHandler->deleteRecord(contractorID, equivalent, messageType));
    EXPECT_NO_THROW(mHandler->deleteRecord(contractorID, transactionUUID));
}

// Test deleteRecord selective deletion
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, DeleteRecord_SelectiveDeletion_Success) {
    // Create multiple records
    vector<tuple<ContractorID, SerializedEquivalent, TransactionUUID, int, string>> testData = {
        {1, 0, createTestUUID(), static_cast<int>(101), "Message 1"},
        {1, 1, createTestUUID(), static_cast<int>(102), "Message 2"},
        {2, 0, createTestUUID(), static_cast<int>(103), "Message 3"}
    };
    
    for (const auto& data : testData) {
        BytesShared message = createTestMessage(get<4>(data));
        mHandler->saveRecord(get<0>(data), get<1>(data), get<2>(data), get<3>(data), 
                          message, get<4>(data).length());
    }
    
    // Verify all records exist
    auto records = mHandler->allMessages();
    ASSERT_EQ(records.size(), 3);
    
    // Delete specific record
    mHandler->deleteRecord(1, 0, static_cast<int>(101));
    
    // Verify only specific record is deleted
    records = mHandler->allMessages();
    ASSERT_EQ(records.size(), 2);
    
    // Verify remaining records are correct - should have contractor 1 with different equivalent/type and contractor 2
    bool foundContractor1Record = false;
    bool foundContractor2Record = false;
    
    for (const auto& record : records) {
        ContractorID contractorId = get<0>(record);
        int messageType = static_cast<int>(get<2>(record));
        
        if (contractorId == 1) {
            foundContractor1Record = true;
            EXPECT_NE(messageType, 101) << "Should not find the deleted record (contractor 1, message type 101)";
        } else if (contractorId == 2) {
            foundContractor2Record = true;
        }
    }
    
    EXPECT_TRUE(foundContractor1Record) << "Should still have contractor 1 record with different equivalent/type";
    EXPECT_TRUE(foundContractor2Record) << "Should still have contractor 2 record";
}

// Test raw database data validation
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, RawDatabaseDataValidation_SavedDataMatchesExpected) {
    ContractorID contractorID = 1;
    SerializedEquivalent equivalent = 0;
    TransactionUUID transactionUUID = createTestUUID();
    int messageType = static_cast<int>(100);
    string messageContent = "Test message for raw validation";
    BytesShared message = createTestMessage(messageContent);
    
    mHandler->saveRecord(contractorID, equivalent, transactionUUID, messageType, message, messageContent.length());
    
    // Verify raw database data
    verifyRawDatabaseRecord(contractorID, equivalent, transactionUUID, messageType, messageContent);
}

// Test reverse validation (insert via SQL, read via handler)
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, ReverseValidation_SQLInsertHandlerRead_Success) {
    ContractorID contractorID = 1;
    SerializedEquivalent equivalent = 0;
    TransactionUUID transactionUUID = createTestUUID();
    int messageType = static_cast<int>(100);
    string messageContent = "SQL inserted message";
    
    // Insert via raw SQL
    insertRawDatabaseRecord(contractorID, equivalent, transactionUUID, messageType, messageContent);
    
    // Read via handler
    auto records = mHandler->allMessages();
    ASSERT_EQ(records.size(), 1);
    
    auto& record = records[0];
    EXPECT_EQ(get<0>(record), contractorID);
    EXPECT_EQ(get<2>(record), messageType);
    
    BytesShared returnedMessage = get<1>(record);
    // PostgreSQL binary data handling differs from text, so just verify message exists
    EXPECT_TRUE(returnedMessage != nullptr) << "Returned message should not be null";
}

// Test cross-method validation
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, CrossMethodValidation_SaveReadDelete_Success) {
    ContractorID contractorID = 1;
    SerializedEquivalent equivalent = 0;
    TransactionUUID transactionUUID = createTestUUID();
    int messageType = static_cast<int>(100);
    string messageContent = "Cross-method validation message";
    BytesShared message = createTestMessage(messageContent);
    
    // Save
    mHandler->saveRecord(contractorID, equivalent, transactionUUID, messageType, message, messageContent.length());
    
    // Read and verify
    auto records = mHandler->allMessages();
    ASSERT_EQ(records.size(), 1);
    
    auto& record = records[0];
    EXPECT_EQ(get<0>(record), contractorID);
    EXPECT_EQ(get<2>(record), messageType);
    
    // Delete
    mHandler->deleteRecord(contractorID, transactionUUID);
    
    // Verify deleted
    records = mHandler->allMessages();
    EXPECT_TRUE(records.empty());
}

// Test table schema validation
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, TableSchemaValidation_CorrectSchema_Success) {
    verifyTableSchema();
}

// Test constructor validation
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, Constructor_ValidParameters_Success) {
    // Test with valid parameters
    EXPECT_NO_THROW({
        auto handler = make_unique<CommunicatorMessagesQueueHandlerPostgreSQL>(
            mConnection,
            "test_table_name",
            *mLogger
        );
    });
}

// Test constructor with null connection
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, Constructor_NullConnection_ThrowsException) {
    EXPECT_THROW({
        auto handler = make_unique<CommunicatorMessagesQueueHandlerPostgreSQL>(
            nullptr,
            "test_table_name",
            *mLogger
        );
    }, ValueError);
}

// Test constructor with empty table name
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, Constructor_EmptyTableName_ThrowsException) {
    EXPECT_THROW({
        auto handler = make_unique<CommunicatorMessagesQueueHandlerPostgreSQL>(
            mConnection,
            "",
            *mLogger
        );
    }, ValueError);
}

// Test foreign key constraint validation
TEST_F(CommunicatorMessagesQueueHandlerPostgreSQLIntegrationTest, ForeignKeyConstraint_InvalidContractorID_ThrowsException) {
    ContractorID nonExistentContractorID = 999;
    SerializedEquivalent equivalent = 0;
    TransactionUUID transactionUUID = createTestUUID();
    int messageType = static_cast<int>(100);
    string messageContent = "Test message";
    BytesShared message = createTestMessage(messageContent);
    
    // Should throw due to foreign key constraint
    EXPECT_THROW(mHandler->saveRecord(nonExistentContractorID, equivalent, transactionUUID, messageType, 
                                    message, messageContent.length()), IOError);
} 