#include "gtest/gtest.h"
#include "../../../../src/core/io/storage/postgresql/OutgoingPaymentReceiptHandlerPostgreSQL.h"
#include "../../../../src/core/logger/Logger.h"
#include "../../../../src/core/io/storage/record/audit/ReceiptRecord.h"
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


class OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest : public ::testing::Test {
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
        mTestTableName = "outgoing_receipt_test_" + std::to_string(testCounter++);
        
        // Create required tables with foreign key constraints
        createRequiredTables();
        
        // Create test trust line and own key records
        insertTestData();
        
        // Create OutgoingPaymentReceiptHandlerPostgreSQL instance
        mHandler = std::make_unique<OutgoingPaymentReceiptHandlerPostgreSQL>(
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
    
    void createRequiredTables() {
        // Create trust_lines table (required by foreign key constraint)
        std::string query = "CREATE TABLE IF NOT EXISTS trust_lines ("
                           "id INTEGER PRIMARY KEY, "
                           "contractor_id INTEGER, "
                           "equivalent INTEGER, "
                           "state INTEGER, "
                           "is_contractor_gateway INTEGER)";
        DatabaseTestHelper::executeQuery(mConnection, query);
        
        // Create own_keys table (required by foreign key constraint)
        query = "CREATE TABLE IF NOT EXISTS own_keys ("
               "hash BYTEA PRIMARY KEY, "
               "trust_line_id INTEGER, "
               "number INTEGER, "
               "public_key BYTEA, "
               "private_key BYTEA)";
        DatabaseTestHelper::executeQuery(mConnection, query);
    }
    
    void insertTestData() {
        // Insert test trust lines
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
        
        // Insert test own keys (including additional keys used by various tests)
        std::vector<std::string> keyNames = {
            "testOwnKey", "testOwnKey1", "testOwnKey2", "testOwnKey3"
        };
        
        std::string query = "INSERT INTO own_keys (hash, trust_line_id, number, public_key, private_key) VALUES ";
        std::vector<std::string> values;
        
        for (size_t i = 0; i < keyNames.size(); ++i) {
            auto testKeyHash = createTestKeyHash(keyNames[i]);
            std::string hexKeyHash = bytesToHexString(testKeyHash->data(), KeyHash::kBytesSize);
            
            values.push_back("(decode('" + hexKeyHash + "', 'hex'), " + 
                           std::to_string(getValidTrustLineID()) + ", " + 
                           std::to_string(i + 1) + ", " +
                           "decode('" + hexKeyHash + "', 'hex'), " +
                           "decode('" + hexKeyHash + "', 'hex'))");
        }
        
        query += values[0];
        for (size_t i = 1; i < values.size(); ++i) {
            query += ", " + values[i];
        }
        query += " ON CONFLICT (hash) DO NOTHING";
        
        DatabaseTestHelper::executeQuery(mConnection, query);
    }
    
    void cleanupTestData() {
        try {
            DatabaseTestHelper::cleanupTable(mConnection, mTestTableName);
            DatabaseTestHelper::cleanupTable(mConnection, "trust_lines");
            DatabaseTestHelper::cleanupTable(mConnection, "own_keys");
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
    
    KeyHash::Shared createTestKeyHash(const std::string& testData) {
        // Create a deterministic key hash for testing
        byte_t hashData[KeyHash::kBytesSize];
        memset(hashData, 0, KeyHash::kBytesSize);
        
        // Fill with test data
        size_t dataSize = std::min(testData.length(), static_cast<size_t>(KeyHash::kBytesSize));
        memcpy(hashData, testData.c_str(), dataSize);
        
        return std::make_shared<KeyHash>(hashData);
    }
    
    TrustLineAmount createTestAmount(long long value) {
        return TrustLineAmount(value);
    }
    
    TransactionUUID createTestTransactionUUID(const std::string& testData) {
        TransactionUUID uuid;
        memset(uuid.data, 0, TransactionUUID::kBytesSize);
        
        size_t dataSize = std::min(testData.length(), static_cast<size_t>(TransactionUUID::kBytesSize));
        memcpy(uuid.data, testData.c_str(), dataSize);
        
        return uuid;
    }
    
    std::string bytesToHexString(const byte_t* data, size_t size) {
        std::ostringstream oss;
        for (size_t i = 0; i < size; ++i) {
            oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(data[i]);
        }
        return oss.str();
    }
    
    int getReceiptCount(TrustLineID trustLineID) {
        std::string query = "SELECT COUNT(*) FROM " + mTestTableName + " WHERE trust_line_id = " + std::to_string(trustLineID);
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get receipt count");
        }
        
        int count = std::stoi(PQgetvalue(result, 0, 0));
        PQclear(result);
        return count;
    }
    
    // Helper method to verify raw database data
    struct RawReceiptData {
        int trustLineId;
        int auditNumber;
        std::string transactionUuidHex;
        std::string ownPublicKeyHashHex;
        std::string amountHex;
    };
    
    std::vector<RawReceiptData> getRawReceiptData(TrustLineID trustLineID) {
        std::string query = "SELECT trust_line_id, audit_number, "
                           "encode(transaction_uuid, 'hex'), encode(own_public_key_hash, 'hex'), "
                           "encode(amount, 'hex') "
                           "FROM " + mTestTableName + " WHERE trust_line_id = " + std::to_string(trustLineID) + " ORDER BY audit_number";
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get raw receipt data");
        }
        
        std::vector<RawReceiptData> data;
        int rows = PQntuples(result);
        
        for (int i = 0; i < rows; ++i) {
            RawReceiptData rawData;
            rawData.trustLineId = std::stoi(PQgetvalue(result, i, 0));
            rawData.auditNumber = std::stoi(PQgetvalue(result, i, 1));
            rawData.transactionUuidHex = PQgetvalue(result, i, 2);
            rawData.ownPublicKeyHashHex = PQgetvalue(result, i, 3);
            rawData.amountHex = PQgetvalue(result, i, 4);
            data.push_back(rawData);
        }
        
        PQclear(result);
        return data;
    }
    
    void insertReceiptViaSQL(TrustLineID trustLineID, AuditNumber auditNumber, 
                            const std::string& transactionUuidHex, const std::string& ownKeyHashHex,
                            const std::string& amountHex) {
        std::string query = "INSERT INTO " + mTestTableName + 
                           " (trust_line_id, audit_number, transaction_uuid, own_public_key_hash, amount) "
                           "VALUES (" + std::to_string(trustLineID) + ", " + std::to_string(auditNumber) + ", "
                           "decode('" + transactionUuidHex + "', 'hex'), decode('" + ownKeyHashHex + "', 'hex'), "
                           "decode('" + amountHex + "', 'hex'))";
        
        DatabaseTestHelper::executeQuery(mConnection, query);
    }

protected:
    PGconn* mConnection;
    std::unique_ptr<OutgoingPaymentReceiptHandlerPostgreSQL> mHandler;
    Logger mLogger;
    std::string mTestTableName;
    static int testCounter;
};

// Initialize static counter
int OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest::testCounter = 0;

// Test saveRecord method
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, saveRecord_ValidData_SavesSuccessfully) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto ownPublicKeyHash = createTestKeyHash("testOwnKey");
    auto amount = createTestAmount(1000);
    
    // Test the method
    EXPECT_NO_THROW(
        mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, ownPublicKeyHash, amount)
    );
    
    // Verify data was saved
    EXPECT_EQ(getReceiptCount(trustLineID), 1);
    
    // Verify raw database data
    auto rawData = getRawReceiptData(trustLineID);
    EXPECT_EQ(rawData.size(), 1);
    EXPECT_EQ(rawData[0].trustLineId, trustLineID);
    EXPECT_EQ(rawData[0].auditNumber, auditNumber);
    EXPECT_FALSE(rawData[0].transactionUuidHex.empty());
    EXPECT_FALSE(rawData[0].ownPublicKeyHashHex.empty());
    EXPECT_FALSE(rawData[0].amountHex.empty());
}

// Test saveRecord with null ownPublicKeyHash
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, saveRecord_NullOwnKeyHash_ThrowsException) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto amount = createTestAmount(1000);
    
    EXPECT_THROW(
        mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, nullptr, amount),
        ValueError
    );
}

// Test auditAmounts method
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, auditAmounts_ValidData_ReturnsCorrectAmounts) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID1 = createTestTransactionUUID("testTxUUID1");
    auto transactionUUID2 = createTestTransactionUUID("testTxUUID2");
    auto ownPublicKeyHash1 = createTestKeyHash("testOwnKey1");
    auto ownPublicKeyHash2 = createTestKeyHash("testOwnKey2");
    auto amount1 = createTestAmount(1000);
    auto amount2 = createTestAmount(2000);
    
    // Insert test data with different own keys to avoid unique constraint violation
    mHandler->saveRecord(trustLineID, auditNumber, transactionUUID1, ownPublicKeyHash1, amount1);
    mHandler->saveRecord(trustLineID, auditNumber, transactionUUID2, ownPublicKeyHash2, amount2);
    
    // Test the method
    auto auditAmounts = mHandler->auditAmounts(trustLineID, auditNumber);
    
    // Verify results
    EXPECT_EQ(auditAmounts.size(), 2);
    EXPECT_EQ(auditAmounts[transactionUUID1], amount1);
    EXPECT_EQ(auditAmounts[transactionUUID2], amount2);
}

// Test auditAmounts with no data
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, auditAmounts_NoData_ReturnsEmptyMap) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    
    auto auditAmounts = mHandler->auditAmounts(trustLineID, auditNumber);
    
    EXPECT_TRUE(auditAmounts.empty());
}

// Test receiptsByAuditNumber method
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, receiptsByAuditNumber_ValidData_ReturnsCorrectReceipts) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto ownPublicKeyHash = createTestKeyHash("testOwnKey");
    auto amount = createTestAmount(1000);
    
    // Insert test data
    mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, ownPublicKeyHash, amount);
    
    // Test the method
    auto receipts = mHandler->receiptsByAuditNumber(trustLineID, auditNumber);
    
    // Verify results
    EXPECT_EQ(receipts.size(), 1);
    EXPECT_EQ(receipts[0]->auditNumber(), auditNumber);
    EXPECT_EQ(receipts[0]->transactionUUID(), transactionUUID);
    EXPECT_EQ(receipts[0]->amount(), amount);
    EXPECT_TRUE(receipts[0]->keyHash() != nullptr);
    EXPECT_TRUE(receipts[0]->signature() == nullptr); // OutgoingPaymentReceipt doesn't have signature
}

// Test receiptsByAuditNumber with no data
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, receiptsByAuditNumber_NoData_ReturnsEmptyVector) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    
    auto receipts = mHandler->receiptsByAuditNumber(trustLineID, auditNumber);
    
    EXPECT_TRUE(receipts.empty());
}

// Test receiptsLessEqualThanAuditNumber method
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, receiptsLessEqualThanAuditNumber_ValidData_ReturnsCorrectReceipts) {
    TrustLineID trustLineID = getValidTrustLineID();
    auto amount = createTestAmount(1000);
    
    // Insert test data with different audit numbers, using existing own keys (0-3)
    std::vector<std::string> keyNames = {"testOwnKey", "testOwnKey1", "testOwnKey2", "testOwnKey3"};
    for (size_t i = 0; i < keyNames.size(); ++i) {
        auto txUUID = createTestTransactionUUID("testTxUUID" + std::to_string(i + 1));
        auto ownKeyHash = createTestKeyHash(keyNames[i]);
        mHandler->saveRecord(trustLineID, i + 1, txUUID, ownKeyHash, amount);
    }
    
    // Test the method
    auto receipts = mHandler->receiptsLessEqualThanAuditNumber(trustLineID, 3);
    
    // Verify results
    EXPECT_EQ(receipts.size(), 3);
    
    // Verify audit numbers are <= 3
    for (const auto& receipt : receipts) {
        EXPECT_LE(receipt->auditNumber(), 3);
    }
}

// Test receiptsLessEqualThanAuditNumber with no data
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, receiptsLessEqualThanAuditNumber_NoData_ReturnsEmptyVector) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 3;
    
    auto receipts = mHandler->receiptsLessEqualThanAuditNumber(trustLineID, auditNumber);
    
    EXPECT_TRUE(receipts.empty());
}

// Test deleteRecords by TransactionUUID
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, deleteRecords_ValidTransactionUUID_DeletesSuccessfully) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto ownPublicKeyHash = createTestKeyHash("testOwnKey");
    auto amount = createTestAmount(1000);
    
    // Insert test data
    mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, ownPublicKeyHash, amount);
    
    // Verify data exists
    EXPECT_EQ(getReceiptCount(trustLineID), 1);
    
    // Test the method
    EXPECT_NO_THROW(mHandler->deleteRecords(transactionUUID));
    
    // Verify data was deleted
    EXPECT_EQ(getReceiptCount(trustLineID), 0);
}

// Test deleteRecords by TrustLineID
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, deleteRecords_ValidTrustLineID_DeletesSuccessfully) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto ownPublicKeyHash = createTestKeyHash("testOwnKey");
    auto amount = createTestAmount(1000);
    
    // Insert test data
    mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, ownPublicKeyHash, amount);
    
    // Verify data exists
    EXPECT_EQ(getReceiptCount(trustLineID), 1);
    
    // Test the method
    EXPECT_NO_THROW(mHandler->deleteRecords(trustLineID));
    
    // Verify data was deleted
    EXPECT_EQ(getReceiptCount(trustLineID), 0);
}

// Test isContainsKeyHash method
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, isContainsKeyHash_ValidData_ReturnsTrue) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto ownPublicKeyHash = createTestKeyHash("testOwnKey");
    auto amount = createTestAmount(1000);
    
    // Insert test data
    mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, ownPublicKeyHash, amount);
    
    // Test the method
    EXPECT_TRUE(mHandler->isContainsKeyHash(ownPublicKeyHash));
    
    // Test with non-existent key hash
    auto nonExistentKeyHash = createTestKeyHash("nonExistentKey");
    EXPECT_FALSE(mHandler->isContainsKeyHash(nonExistentKeyHash));
}

// Test isContainsKeyHash with null keyHash
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, isContainsKeyHash_NullKeyHash_ThrowsException) {
    EXPECT_THROW(
        mHandler->isContainsKeyHash(nullptr),
        ValueError
    );
}

// Test isContainsTransaction method
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, isContainsTransaction_ValidData_ReturnsTrue) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto ownPublicKeyHash = createTestKeyHash("testOwnKey");
    auto amount = createTestAmount(1000);
    
    // Insert test data
    mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, ownPublicKeyHash, amount);
    
    // Test the method
    EXPECT_TRUE(mHandler->isContainsTransaction(transactionUUID));
    
    // Test with non-existent transaction UUID
    auto nonExistentUUID = createTestTransactionUUID("nonExistentTxUUID");
    EXPECT_FALSE(mHandler->isContainsTransaction(nonExistentUUID));
}

// Test countReceiptsByNumber method
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, countReceiptsByNumber_ValidData_ReturnsCorrectCount) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto amount = createTestAmount(1000);
    
    // Insert multiple records with same audit number but different own keys
    for (int i = 1; i <= 3; ++i) {
        auto transactionUUID = createTestTransactionUUID("testTxUUID" + std::to_string(i));
        auto ownPublicKeyHash = createTestKeyHash("testOwnKey" + std::to_string(i));
        mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, ownPublicKeyHash, amount);
    }
    
    // Test the method
    auto count = mHandler->countReceiptsByNumber(trustLineID, auditNumber);
    
    // Verify results
    EXPECT_EQ(count, 3);
}

// Test countReceiptsByNumber with no data
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, countReceiptsByNumber_NoData_ReturnsZero) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    
    auto count = mHandler->countReceiptsByNumber(trustLineID, auditNumber);
    
    EXPECT_EQ(count, 0);
}

// Test Raw Database Data Validation
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, RawDataValidation_SaveRecord_CorrectDatabaseStorage) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto ownPublicKeyHash = createTestKeyHash("testOwnKey");
    auto amount = createTestAmount(1000);
    
    // Save record using the class method
    mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, ownPublicKeyHash, amount);
    
    // Verify raw database data using direct SQL queries
    std::string countQuery = "SELECT COUNT(*) FROM " + mTestTableName + " WHERE trust_line_id = " + std::to_string(trustLineID);
    PGresult* result = PQexec(mConnection, countQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    EXPECT_EQ(std::stoi(PQgetvalue(result, 0, 0)), 1);
    PQclear(result);
    
    // Verify specific field values
    std::string dataQuery = "SELECT trust_line_id, audit_number, "
                           "encode(transaction_uuid, 'hex'), encode(own_public_key_hash, 'hex'), "
                           "encode(amount, 'hex') "
                           "FROM " + mTestTableName + " WHERE trust_line_id = " + std::to_string(trustLineID);
    result = PQexec(mConnection, dataQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    EXPECT_EQ(std::stoi(PQgetvalue(result, 0, 0)), trustLineID);
    EXPECT_EQ(std::stoi(PQgetvalue(result, 0, 1)), auditNumber);
    
    // Verify binary data is stored correctly
    std::string storedUuidHex = PQgetvalue(result, 0, 2);
    std::string storedKeyHashHex = PQgetvalue(result, 0, 3);
    std::string storedAmountHex = PQgetvalue(result, 0, 4);
    
    EXPECT_FALSE(storedUuidHex.empty());
    EXPECT_FALSE(storedKeyHashHex.empty());
    EXPECT_FALSE(storedAmountHex.empty());
    
    PQclear(result);
}

// Test Reverse Validation: Insert via SQL, read via class methods
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, ReverseValidation_InsertViaSQL_ReadViaClass) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    
    // Use an existing own key hash to avoid foreign key constraint violation
    auto testKeyHash = createTestKeyHash("testOwnKey");
    std::string ownKeyHashHex = bytesToHexString(testKeyHash->data(), KeyHash::kBytesSize);
    
    // Generate other test data as hex strings
    std::string transactionUuidHex = "0000000000000000000000000000000000000000000000000000000000000001";
    std::string amountHex = "00000000000000000000000000000000000000000000000000000000000003e8"; // 1000 in amount format
    
    // Insert data via SQL
    insertReceiptViaSQL(trustLineID, auditNumber, transactionUuidHex, ownKeyHashHex, amountHex);
    
    // Read data via class methods
    auto receipts = mHandler->receiptsByAuditNumber(trustLineID, auditNumber);
    
    // Verify results
    EXPECT_EQ(receipts.size(), 1);
    EXPECT_EQ(receipts[0]->auditNumber(), auditNumber);
    EXPECT_TRUE(receipts[0]->keyHash() != nullptr);
    EXPECT_TRUE(receipts[0]->signature() == nullptr); // OutgoingPaymentReceipt doesn't have signature
    
    // Verify audit amounts map
    auto auditAmounts = mHandler->auditAmounts(trustLineID, auditNumber);
    EXPECT_EQ(auditAmounts.size(), 1);
    
    // Verify count method
    auto count = mHandler->countReceiptsByNumber(trustLineID, auditNumber);
    EXPECT_EQ(count, 1);
}

// Test Constructor with null connection
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, Constructor_NullConnection_ThrowsException) {
    EXPECT_THROW(
        OutgoingPaymentReceiptHandlerPostgreSQL(nullptr, "test_table", mLogger),
        ValueError
    );
}

// Test Constructor with empty table name
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, Constructor_EmptyTableName_ThrowsException) {
    EXPECT_THROW(
        OutgoingPaymentReceiptHandlerPostgreSQL(mConnection, "", mLogger),
        ValueError
    );
}

// Test table creation and schema validation
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, TableCreation_ValidatesSchemaCorrectly) {
    // Test that the table was created with correct schema
    std::string schemaQuery = "SELECT column_name, data_type FROM information_schema.columns WHERE table_name = '" + mTestTableName + "' ORDER BY ordinal_position";
    PGresult* result = PQexec(mConnection, schemaQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    
    // Verify we have the expected columns
    int columnCount = PQntuples(result);
    EXPECT_EQ(columnCount, 5); // trust_line_id, audit_number, transaction_uuid, own_public_key_hash, amount
    
    // Verify some key columns exist
    std::vector<std::string> expectedColumns = {"trust_line_id", "audit_number", "transaction_uuid", "own_public_key_hash", "amount"};
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

// Test unique constraint validation
TEST_F(OutgoingPaymentReceiptHandlerPostgreSQLIntegrationTest, UniqueConstraint_DuplicateKey_ThrowsException) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto ownPublicKeyHash = createTestKeyHash("testOwnKey");
    auto amount = createTestAmount(1000);
    
    // Insert first record
    mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, ownPublicKeyHash, amount);
    
    // Try to insert duplicate with same trust_line_id, audit_number, own_public_key_hash
    EXPECT_THROW(
        mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, ownPublicKeyHash, amount),
        IOError
    );
} 