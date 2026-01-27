#include "gtest/gtest.h"
#include "../../../../src/core/io/storage/postgresql/IncomingPaymentReceiptHandlerPostgreSQL.h"
#include "../../../../src/core/logger/Logger.h"
#include "../../../../src/core/io/storage/record/audit/ReceiptRecord.h"
#include "../../../../src/core/transactions/transactions/base/TransactionUUID.h"
#include "../../../../src/core/io/storage/interfaces/PaymentTransactionsHandler.h"
#include "../../../../src/core/io/storage/postgresql/StorageHandlerPostgreSQL.h"
#include "../fixtures/DatabaseTestHelper.h"
#include "../fixtures/PostgreSQLTestFixtures.h"
#include <memory>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <libpq-fe.h>
#include <sodium.h>


class IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest : public ::testing::Test {
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
        mTestTableName = "incoming_receipt_test_" + std::to_string(testCounter++);
        
        // Create required tables with foreign key constraints
        createRequiredTables();
        
        // Create test trust line and contractor key records
        insertTestData();
        
        // Create IncomingPaymentReceiptHandlerPostgreSQL instance
        mHandler = std::make_unique<IncomingPaymentReceiptHandlerPostgreSQL>(
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
        
        // Create contractor_keys table (required by foreign key constraint)
        query = "CREATE TABLE IF NOT EXISTS contractor_keys ("
               "hash BYTEA PRIMARY KEY, "
               "contractor_id INTEGER, "
               "trust_line_id INTEGER, "
               "number INTEGER, "
               "public_key BYTEA)";
        DatabaseTestHelper::executeQuery(mConnection, query);

        // Create payment_keys table (required by payment_transactions foreign key).
        query = "CREATE TABLE IF NOT EXISTS payment_keys ("
               "id BIGSERIAL PRIMARY KEY, "
               "public_key BYTEA NOT NULL, "
               "private_key BYTEA NOT NULL)";
        DatabaseTestHelper::executeQuery(mConnection, query);

        // Create payment_transactions table (used for finalized receipt selection).
        query = "CREATE TABLE IF NOT EXISTS " + std::string(StorageHandlerPostgreSQL::paymentTransactionsTableName()) +
               " (uuid BYTEA NOT NULL, "
               "maximal_claiming_block_number BIGINT NOT NULL, "
               "effective_claiming_block_number BIGINT NOT NULL, "
               "observing_state INTEGER NOT NULL, "
               "recording_time BIGINT NOT NULL, "
               "payment_key_id BIGINT NOT NULL, "
               "FOREIGN KEY(payment_key_id) REFERENCES payment_keys(id))";
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
        
        // Insert test contractor keys (including additional keys used by various tests)
        std::vector<std::string> keyNames = {
            "testContractorKey", "testContractorKey1", "testContractorKey2", "testContractorKey3"
        };
        
        std::string query = "INSERT INTO contractor_keys (hash, contractor_id, trust_line_id, number, public_key) VALUES ";
        std::vector<std::string> values;
        
        for (size_t i = 0; i < keyNames.size(); ++i) {
            auto testKeyHash = createTestKeyHash(keyNames[i]);
            std::string hexKeyHash = bytesToHexString(testKeyHash->data(), KeyHash::kBytesSize);
            
            values.push_back("(decode('" + hexKeyHash + "', 'hex'), " + 
                           std::to_string(getValidTrustLineID() * 10 + i) + ", " + 
                           std::to_string(getValidTrustLineID()) + ", " + 
                           std::to_string(i + 1) + ", decode('" + hexKeyHash + "', 'hex'))");
        }
        
        query += values[0];
        for (size_t i = 1; i < values.size(); ++i) {
            query += ", " + values[i];
        }
        query += " ON CONFLICT (hash) DO NOTHING";
        
        DatabaseTestHelper::executeQuery(mConnection, query);

        // Insert a payment key used by payment_transactions.
        query = "INSERT INTO payment_keys (public_key, private_key) VALUES (decode('00', 'hex'), decode('00', 'hex'))";
        DatabaseTestHelper::executeQuery(mConnection, query);
    }
    
    void cleanupTestData() {
        try {
            DatabaseTestHelper::cleanupTable(mConnection, mTestTableName);
            DatabaseTestHelper::cleanupTable(mConnection, "trust_lines");
            DatabaseTestHelper::cleanupTable(mConnection, "contractor_keys");
            DatabaseTestHelper::cleanupTable(mConnection, StorageHandlerPostgreSQL::paymentTransactionsTableName());
            DatabaseTestHelper::cleanupTable(mConnection, "payment_keys");
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
    
    Signature::Shared createTestSignature(const std::string& testData) {
        auto privateKey = std::make_unique<PrivateKey>();
        Signature::Shared sig = std::make_shared<Signature>();
        sig->sign(*privateKey, reinterpret_cast<const byte_t*>(testData.c_str()), testData.length());
        return sig;
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
        std::string contractorPublicKeyHashHex;
        std::string amountHex;
        std::string contractorSignatureHex;
    };
    
    std::vector<RawReceiptData> getRawReceiptData(TrustLineID trustLineID) {
        std::string query = "SELECT trust_line_id, audit_number, "
                           "encode(transaction_uuid, 'hex'), encode(contractor_public_key_hash, 'hex'), "
                           "encode(amount, 'hex'), encode(contractor_signature, 'hex') "
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
            rawData.contractorPublicKeyHashHex = PQgetvalue(result, i, 3);
            rawData.amountHex = PQgetvalue(result, i, 4);
            rawData.contractorSignatureHex = PQgetvalue(result, i, 5);
            data.push_back(rawData);
        }
        
        PQclear(result);
        return data;
    }
    
    void insertReceiptViaSQL(TrustLineID trustLineID, AuditNumber auditNumber, 
                            const std::string& transactionUuidHex, const std::string& contractorKeyHashHex,
                            const std::string& amountHex, const std::string& contractorSignatureHex) {
        std::string query = "INSERT INTO " + mTestTableName + 
                           " (trust_line_id, audit_number, transaction_uuid, contractor_public_key_hash, amount, contractor_signature) "
                           "VALUES (" + std::to_string(trustLineID) + ", " + std::to_string(auditNumber) + ", "
                           "decode('" + transactionUuidHex + "', 'hex'), decode('" + contractorKeyHashHex + "', 'hex'), "
                           "decode('" + amountHex + "', 'hex'), decode('" + contractorSignatureHex + "', 'hex'))";
        
        DatabaseTestHelper::executeQuery(mConnection, query);
    }

    void insertPaymentTransaction(const TransactionUUID &transactionUUID, int observingState) {
        std::string uuidHex = bytesToHexString(transactionUUID.data, TransactionUUID::kBytesSize);
        std::string query = "INSERT INTO " + std::string(StorageHandlerPostgreSQL::paymentTransactionsTableName()) +
                           " (uuid, maximal_claiming_block_number, effective_claiming_block_number, "
                           "observing_state, recording_time, payment_key_id) VALUES ("
                           "decode('" + uuidHex + "', 'hex'), 1, 1, " + std::to_string(observingState) +
                           ", 0, (SELECT id FROM payment_keys ORDER BY id DESC LIMIT 1))";
        DatabaseTestHelper::executeQuery(mConnection, query);
    }

protected:
    PGconn* mConnection;
    std::unique_ptr<IncomingPaymentReceiptHandlerPostgreSQL> mHandler;
    Logger mLogger;
    std::string mTestTableName;
    static int testCounter;
};

// Initialize static counter
int IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest::testCounter = 0;

// Test saveRecord method
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, saveRecord_ValidData_SavesSuccessfully) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto contractorPublicKeyHash = createTestKeyHash("testContractorKey");
    auto amount = createTestAmount(1000);
    auto contractorSignature = createTestSignature("testContractorSignature");
    
    // Test the method
    EXPECT_NO_THROW(
        mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, contractorPublicKeyHash, amount, contractorSignature)
    );
    
    // Verify data was saved
    EXPECT_EQ(getReceiptCount(trustLineID), 1);
    
    // Verify raw database data
    auto rawData = getRawReceiptData(trustLineID);
    EXPECT_EQ(rawData.size(), 1);
    EXPECT_EQ(rawData[0].trustLineId, trustLineID);
    EXPECT_EQ(rawData[0].auditNumber, auditNumber);
    EXPECT_FALSE(rawData[0].transactionUuidHex.empty());
    EXPECT_FALSE(rawData[0].contractorPublicKeyHashHex.empty());
    EXPECT_FALSE(rawData[0].amountHex.empty());
    EXPECT_FALSE(rawData[0].contractorSignatureHex.empty());
}

// Test saveRecord with null contractorPublicKeyHash
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, saveRecord_NullContractorKeyHash_ThrowsException) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto amount = createTestAmount(1000);
    auto contractorSignature = createTestSignature("testContractorSignature");
    
    EXPECT_THROW(
        mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, nullptr, amount, contractorSignature),
        ValueError
    );
}

// Test saveRecord with null contractorSignature
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, saveRecord_NullContractorSignature_ThrowsException) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto contractorPublicKeyHash = createTestKeyHash("testContractorKey");
    auto amount = createTestAmount(1000);
    
    EXPECT_THROW(
        mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, contractorPublicKeyHash, amount, nullptr),
        ValueError
    );
}

// Test auditAmounts method
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, auditAmounts_ValidData_ReturnsCorrectAmounts) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID1 = createTestTransactionUUID("testTxUUID1");
    auto transactionUUID2 = createTestTransactionUUID("testTxUUID2");
    auto contractorPublicKeyHash1 = createTestKeyHash("testContractorKey1");
    auto contractorPublicKeyHash2 = createTestKeyHash("testContractorKey2");
    auto amount1 = createTestAmount(1000);
    auto amount2 = createTestAmount(2000);
    auto contractorSignature = createTestSignature("testContractorSignature");
    
    // Insert test data with different contractor keys to avoid unique constraint violation
    mHandler->saveRecord(trustLineID, auditNumber, transactionUUID1, contractorPublicKeyHash1, amount1, contractorSignature);
    mHandler->saveRecord(trustLineID, auditNumber, transactionUUID2, contractorPublicKeyHash2, amount2, contractorSignature);
    
    // Test the method
    auto auditAmounts = mHandler->auditAmounts(trustLineID, auditNumber);
    
    // Verify results
    EXPECT_EQ(auditAmounts.size(), 2);
    EXPECT_EQ(auditAmounts[transactionUUID1], amount1);
    EXPECT_EQ(auditAmounts[transactionUUID2], amount2);
}

// Test auditAmounts with no data
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, auditAmounts_NoData_ReturnsEmptyMap) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    
    auto auditAmounts = mHandler->auditAmounts(trustLineID, auditNumber);
    
    EXPECT_TRUE(auditAmounts.empty());
}

// Test receiptsByAuditNumber method
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, receiptsByAuditNumber_ValidData_ReturnsCorrectReceipts) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto contractorPublicKeyHash = createTestKeyHash("testContractorKey");
    auto amount = createTestAmount(1000);
    auto contractorSignature = createTestSignature("testContractorSignature");
    
    // Insert test data
    mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, contractorPublicKeyHash, amount, contractorSignature);
    
    // Test the method
    auto receipts = mHandler->receiptsByAuditNumber(trustLineID, auditNumber);
    
    // Verify results
    EXPECT_EQ(receipts.size(), 1);
    EXPECT_EQ(receipts[0]->auditNumber(), auditNumber);
    EXPECT_EQ(receipts[0]->transactionUUID(), transactionUUID);
    EXPECT_EQ(receipts[0]->amount(), amount);
    EXPECT_TRUE(receipts[0]->keyHash() != nullptr);
    EXPECT_TRUE(receipts[0]->signature() != nullptr);
}

// Test receiptsByAuditNumber with no data
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, receiptsByAuditNumber_NoData_ReturnsEmptyVector) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    
    auto receipts = mHandler->receiptsByAuditNumber(trustLineID, auditNumber);
    
    EXPECT_TRUE(receipts.empty());
}

// Test receiptsLessEqualThanAuditNumber method
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, receiptsLessEqualThanAuditNumber_ValidData_ReturnsCorrectReceipts) {
    TrustLineID trustLineID = getValidTrustLineID();
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto contractorPublicKeyHash = createTestKeyHash("testContractorKey");
    auto amount = createTestAmount(1000);
    auto contractorSignature = createTestSignature("testContractorSignature");
    
    // Insert test data with different audit numbers
    for (AuditNumber i = 1; i <= 5; ++i) {
        auto txUUID = createTestTransactionUUID("testTxUUID" + std::to_string(i));
        mHandler->saveRecord(trustLineID, i, txUUID, contractorPublicKeyHash, amount, contractorSignature);
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
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, receiptsLessEqualThanAuditNumber_NoData_ReturnsEmptyVector) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 3;
    
    auto receipts = mHandler->receiptsLessEqualThanAuditNumber(trustLineID, auditNumber);
    
    EXPECT_TRUE(receipts.empty());
}

// Test getFinalizedReceiptsWithZeroAuditNumber method
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, getFinalizedReceiptsWithZeroAuditNumber_FiltersByAuditAndState) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber zeroAudit = 0;
    AuditNumber nonZeroAudit = 1;

    auto committedUUID = createTestTransactionUUID("committedTx");
    auto pendingUUID = createTestTransactionUUID("pendingTx");
    auto missingUUID = createTestTransactionUUID("missingTx");

    insertPaymentTransaction(committedUUID, kCommittedObservingState);
    insertPaymentTransaction(pendingUUID, static_cast<int>(PaymentObservingState::Init));

    auto keyHash1 = createTestKeyHash("testContractorKey");
    auto keyHash2 = createTestKeyHash("testContractorKey1");
    auto keyHash3 = createTestKeyHash("testContractorKey2");
    auto signature = createTestSignature("testContractorSignature");
    auto amount = createTestAmount(1000);

    mHandler->saveRecord(trustLineID, zeroAudit, committedUUID, keyHash1, amount, signature);
    mHandler->saveRecord(trustLineID, zeroAudit, pendingUUID, keyHash2, amount, signature);
    mHandler->saveRecord(trustLineID, zeroAudit, missingUUID, keyHash3, amount, signature);
    mHandler->saveRecord(trustLineID, nonZeroAudit, committedUUID, keyHash1, amount, signature);

    auto receipts = mHandler->getFinalizedReceiptsWithZeroAuditNumber(trustLineID);
    ASSERT_EQ(receipts.size(), 1);
    EXPECT_EQ(receipts[0]->transactionUUID(), committedUUID);
    EXPECT_EQ(receipts[0]->auditNumber(), zeroAudit);
}

TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, getFinalizedReceiptsWithZeroAuditNumber_NoMatches_ReturnsEmptyVector) {
    TrustLineID trustLineID = getValidTrustLineID();
    auto receipts = mHandler->getFinalizedReceiptsWithZeroAuditNumber(trustLineID);
    EXPECT_TRUE(receipts.empty());
}

// Test updateAuditNumberByTransactionUUIDs method
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, updateAuditNumberByTransactionUUIDs_UpdatesSpecifiedReceipts) {
    TrustLineID trustLineID = getValidTrustLineID();
    TrustLineID otherTrustLineID = getValidTrustLineID2();
    AuditNumber zeroAudit = 0;
    AuditNumber updatedAudit = 7;

    auto updateUUID = createTestTransactionUUID("updateTx");
    auto keepUUID = createTestTransactionUUID("keepTx");

    auto keyHash1 = createTestKeyHash("testContractorKey");
    auto keyHash2 = createTestKeyHash("testContractorKey1");
    auto keyHash3 = createTestKeyHash("testContractorKey2");
    auto signature = createTestSignature("testContractorSignature");
    auto amount = createTestAmount(1000);

    mHandler->saveRecord(trustLineID, zeroAudit, updateUUID, keyHash1, amount, signature);
    mHandler->saveRecord(trustLineID, zeroAudit, keepUUID, keyHash2, amount, signature);
    mHandler->saveRecord(otherTrustLineID, zeroAudit, updateUUID, keyHash3, amount, signature);

    mHandler->updateAuditNumberByTransactionUUIDs(trustLineID, updatedAudit, {updateUUID});

    EXPECT_EQ(mHandler->countReceiptsByNumber(trustLineID, updatedAudit), 1);
    EXPECT_EQ(mHandler->countReceiptsByNumber(trustLineID, zeroAudit), 1);
    EXPECT_EQ(mHandler->countReceiptsByNumber(otherTrustLineID, updatedAudit), 0);
    EXPECT_EQ(mHandler->countReceiptsByNumber(otherTrustLineID, zeroAudit), 1);
}

TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, updateAuditNumberByTransactionUUIDs_EmptyList_DoesNotChangeReceipts) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber zeroAudit = 0;
    AuditNumber updatedAudit = 7;

    auto transactionUUID = createTestTransactionUUID("emptyListTx");
    auto keyHash = createTestKeyHash("testContractorKey");
    auto signature = createTestSignature("testContractorSignature");
    auto amount = createTestAmount(1000);

    mHandler->saveRecord(trustLineID, zeroAudit, transactionUUID, keyHash, amount, signature);
    mHandler->updateAuditNumberByTransactionUUIDs(trustLineID, updatedAudit, {});

    EXPECT_EQ(mHandler->countReceiptsByNumber(trustLineID, zeroAudit), 1);
    EXPECT_EQ(mHandler->countReceiptsByNumber(trustLineID, updatedAudit), 0);
}

// Test deleteRecords by TransactionUUID
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, deleteRecords_ValidTransactionUUID_DeletesSuccessfully) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto contractorPublicKeyHash = createTestKeyHash("testContractorKey");
    auto amount = createTestAmount(1000);
    auto contractorSignature = createTestSignature("testContractorSignature");
    
    // Insert test data
    mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, contractorPublicKeyHash, amount, contractorSignature);
    
    // Verify data exists
    EXPECT_EQ(getReceiptCount(trustLineID), 1);
    
    // Test the method
    EXPECT_NO_THROW(mHandler->deleteRecords(transactionUUID));
    
    // Verify data was deleted
    EXPECT_EQ(getReceiptCount(trustLineID), 0);
}

// Test deleteRecords by TrustLineID
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, deleteRecords_ValidTrustLineID_DeletesSuccessfully) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto contractorPublicKeyHash = createTestKeyHash("testContractorKey");
    auto amount = createTestAmount(1000);
    auto contractorSignature = createTestSignature("testContractorSignature");
    
    // Insert test data
    mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, contractorPublicKeyHash, amount, contractorSignature);
    
    // Verify data exists
    EXPECT_EQ(getReceiptCount(trustLineID), 1);
    
    // Test the method
    EXPECT_NO_THROW(mHandler->deleteRecords(trustLineID));
    
    // Verify data was deleted
    EXPECT_EQ(getReceiptCount(trustLineID), 0);
}

// Test deleteRecordsByAuditNumber method
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, deleteRecordsByAuditNumber_deletesMatchingRecords) {
    const TrustLineID trustLineID = getValidTrustLineID();
    const AuditNumber auditToDelete = 5;
    const size_t expectedDeletedCount = 2;

    auto transactionUUID1 = createTestTransactionUUID("deleteAuditTx1");
    auto transactionUUID2 = createTestTransactionUUID("deleteAuditTx2");
    auto contractorPublicKeyHash1 = createTestKeyHash("testContractorKey");
    auto contractorPublicKeyHash2 = createTestKeyHash("testContractorKey1");
    auto amount = createTestAmount(1000);
    auto contractorSignature = createTestSignature("testContractorSignature");

    mHandler->saveRecord(trustLineID, auditToDelete, transactionUUID1, contractorPublicKeyHash1, amount, contractorSignature);
    mHandler->saveRecord(trustLineID, auditToDelete, transactionUUID2, contractorPublicKeyHash2, amount, contractorSignature);

    EXPECT_EQ(mHandler->countReceiptsByNumber(trustLineID, auditToDelete), expectedDeletedCount);

    EXPECT_NO_THROW(mHandler->deleteRecordsByAuditNumber(trustLineID, auditToDelete));

    EXPECT_TRUE(mHandler->receiptsByAuditNumber(trustLineID, auditToDelete).empty());
    EXPECT_EQ(mHandler->countReceiptsByNumber(trustLineID, auditToDelete), 0);
}

TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, deleteRecordsByAuditNumber_doesNotDeleteOtherAuditNumbers) {
    const TrustLineID trustLineID = getValidTrustLineID();
    const AuditNumber auditToDelete = 5;
    const AuditNumber auditToKeep = 6;
    const size_t expectedKeepCount = 2;

    auto deleteUUID1 = createTestTransactionUUID("deleteAuditTx1");
    auto deleteUUID2 = createTestTransactionUUID("deleteAuditTx2");
    auto keepUUID1 = createTestTransactionUUID("keepAuditTx1");
    auto keepUUID2 = createTestTransactionUUID("keepAuditTx2");
    auto contractorKey1 = createTestKeyHash("testContractorKey");
    auto contractorKey2 = createTestKeyHash("testContractorKey1");
    auto contractorKey3 = createTestKeyHash("testContractorKey2");
    auto contractorKey4 = createTestKeyHash("testContractorKey3");
    auto amount = createTestAmount(1000);
    auto contractorSignature = createTestSignature("testContractorSignature");

    mHandler->saveRecord(trustLineID, auditToDelete, deleteUUID1, contractorKey1, amount, contractorSignature);
    mHandler->saveRecord(trustLineID, auditToDelete, deleteUUID2, contractorKey2, amount, contractorSignature);
    mHandler->saveRecord(trustLineID, auditToKeep, keepUUID1, contractorKey3, amount, contractorSignature);
    mHandler->saveRecord(trustLineID, auditToKeep, keepUUID2, contractorKey4, amount, contractorSignature);

    EXPECT_NO_THROW(mHandler->deleteRecordsByAuditNumber(trustLineID, auditToDelete));

    EXPECT_TRUE(mHandler->receiptsByAuditNumber(trustLineID, auditToDelete).empty());
    EXPECT_EQ(mHandler->countReceiptsByNumber(trustLineID, auditToKeep), expectedKeepCount);
}

TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, deleteRecordsByAuditNumber_doesNotDeleteOtherTrustLines) {
    const TrustLineID trustLineID = getValidTrustLineID();
    const TrustLineID otherTrustLineID = getValidTrustLineID2();
    const AuditNumber auditToDelete = 5;
    const size_t expectedOtherCount = 2;

    auto deleteUUID1 = createTestTransactionUUID("deleteAuditTx1");
    auto deleteUUID2 = createTestTransactionUUID("deleteAuditTx2");
    auto keepUUID1 = createTestTransactionUUID("otherAuditTx1");
    auto keepUUID2 = createTestTransactionUUID("otherAuditTx2");
    auto contractorKey1 = createTestKeyHash("testContractorKey");
    auto contractorKey2 = createTestKeyHash("testContractorKey1");
    auto contractorKey3 = createTestKeyHash("testContractorKey2");
    auto contractorKey4 = createTestKeyHash("testContractorKey3");
    auto amount = createTestAmount(1000);
    auto contractorSignature = createTestSignature("testContractorSignature");

    mHandler->saveRecord(trustLineID, auditToDelete, deleteUUID1, contractorKey1, amount, contractorSignature);
    mHandler->saveRecord(trustLineID, auditToDelete, deleteUUID2, contractorKey2, amount, contractorSignature);
    mHandler->saveRecord(otherTrustLineID, auditToDelete, keepUUID1, contractorKey3, amount, contractorSignature);
    mHandler->saveRecord(otherTrustLineID, auditToDelete, keepUUID2, contractorKey4, amount, contractorSignature);

    EXPECT_NO_THROW(mHandler->deleteRecordsByAuditNumber(trustLineID, auditToDelete));

    EXPECT_TRUE(mHandler->receiptsByAuditNumber(trustLineID, auditToDelete).empty());
    EXPECT_EQ(mHandler->countReceiptsByNumber(otherTrustLineID, auditToDelete), expectedOtherCount);
}

// Test isContainsKeyHash method
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, isContainsKeyHash_ValidData_ReturnsTrue) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto contractorPublicKeyHash = createTestKeyHash("testContractorKey");
    auto amount = createTestAmount(1000);
    auto contractorSignature = createTestSignature("testContractorSignature");
    
    // Insert test data
    mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, contractorPublicKeyHash, amount, contractorSignature);
    
    // Test the method
    EXPECT_TRUE(mHandler->isContainsKeyHash(contractorPublicKeyHash));
    
    // Test with non-existent key hash
    auto nonExistentKeyHash = createTestKeyHash("nonExistentKey");
    EXPECT_FALSE(mHandler->isContainsKeyHash(nonExistentKeyHash));
}

// Test isContainsKeyHash with null keyHash
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, isContainsKeyHash_NullKeyHash_ThrowsException) {
    EXPECT_THROW(
        mHandler->isContainsKeyHash(nullptr),
        ValueError
    );
}

// Test isContainsTransaction method
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, isContainsTransaction_ValidData_ReturnsTrue) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto contractorPublicKeyHash = createTestKeyHash("testContractorKey");
    auto amount = createTestAmount(1000);
    auto contractorSignature = createTestSignature("testContractorSignature");
    
    // Insert test data
    mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, contractorPublicKeyHash, amount, contractorSignature);
    
    // Test the method
    EXPECT_TRUE(mHandler->isContainsTransaction(transactionUUID));
    
    // Test with non-existent transaction UUID
    auto nonExistentUUID = createTestTransactionUUID("nonExistentTxUUID");
    EXPECT_FALSE(mHandler->isContainsTransaction(nonExistentUUID));
}

// Test countReceiptsByNumber method
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, countReceiptsByNumber_ValidData_ReturnsCorrectCount) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto amount = createTestAmount(1000);
    auto contractorSignature = createTestSignature("testContractorSignature");
    
    // Insert multiple records with same audit number but different contractor keys
    for (int i = 1; i <= 3; ++i) {
        auto transactionUUID = createTestTransactionUUID("testTxUUID" + std::to_string(i));
        auto contractorPublicKeyHash = createTestKeyHash("testContractorKey" + std::to_string(i));
        mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, contractorPublicKeyHash, amount, contractorSignature);
    }
    
    // Test the method
    auto count = mHandler->countReceiptsByNumber(trustLineID, auditNumber);
    
    // Verify results
    EXPECT_EQ(count, 3);
}

// Test countReceiptsByNumber with no data
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, countReceiptsByNumber_NoData_ReturnsZero) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    
    auto count = mHandler->countReceiptsByNumber(trustLineID, auditNumber);
    
    EXPECT_EQ(count, 0);
}

// Test Raw Database Data Validation
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, RawDataValidation_SaveRecord_CorrectDatabaseStorage) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    auto transactionUUID = createTestTransactionUUID("testTxUUID");
    auto contractorPublicKeyHash = createTestKeyHash("testContractorKey");
    auto amount = createTestAmount(1000);
    auto contractorSignature = createTestSignature("testContractorSignature");
    
    // Save record using the class method
    mHandler->saveRecord(trustLineID, auditNumber, transactionUUID, contractorPublicKeyHash, amount, contractorSignature);
    
    // Verify raw database data using direct SQL queries
    std::string countQuery = "SELECT COUNT(*) FROM " + mTestTableName + " WHERE trust_line_id = " + std::to_string(trustLineID);
    PGresult* result = PQexec(mConnection, countQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    EXPECT_EQ(std::stoi(PQgetvalue(result, 0, 0)), 1);
    PQclear(result);
    
    // Verify specific field values
    std::string dataQuery = "SELECT trust_line_id, audit_number, "
                           "encode(transaction_uuid, 'hex'), encode(contractor_public_key_hash, 'hex'), "
                           "encode(amount, 'hex'), encode(contractor_signature, 'hex') "
                           "FROM " + mTestTableName + " WHERE trust_line_id = " + std::to_string(trustLineID);
    result = PQexec(mConnection, dataQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    EXPECT_EQ(std::stoi(PQgetvalue(result, 0, 0)), trustLineID);
    EXPECT_EQ(std::stoi(PQgetvalue(result, 0, 1)), auditNumber);
    
    // Verify binary data is stored correctly
    std::string storedUuidHex = PQgetvalue(result, 0, 2);
    std::string storedKeyHashHex = PQgetvalue(result, 0, 3);
    std::string storedAmountHex = PQgetvalue(result, 0, 4);
    std::string storedSignatureHex = PQgetvalue(result, 0, 5);
    
    EXPECT_FALSE(storedUuidHex.empty());
    EXPECT_FALSE(storedKeyHashHex.empty());
    EXPECT_FALSE(storedAmountHex.empty());
    EXPECT_FALSE(storedSignatureHex.empty());
    
    PQclear(result);
}

// Test Reverse Validation: Insert via SQL, read via class methods
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, ReverseValidation_InsertViaSQL_ReadViaClass) {
    TrustLineID trustLineID = getValidTrustLineID();
    AuditNumber auditNumber = 1;
    
    // Use an existing contractor key hash to avoid foreign key constraint violation
    auto testKeyHash = createTestKeyHash("testContractorKey");
    std::string contractorKeyHashHex = bytesToHexString(testKeyHash->data(), KeyHash::kBytesSize);
    
    // Generate other test data as hex strings
    std::string transactionUuidHex = "0000000000000000000000000000000000000000000000000000000000000001";
    std::string amountHex = "00000000000000000000000000000000000000000000000000000000000003e8"; // 1000 in amount format
    std::string contractorSignatureHex = "0000000000000000000000000000000000000000000000000000000000000003";
    
    // Insert data via SQL
    insertReceiptViaSQL(trustLineID, auditNumber, transactionUuidHex, contractorKeyHashHex, amountHex, contractorSignatureHex);
    
    // Read data via class methods
    auto receipts = mHandler->receiptsByAuditNumber(trustLineID, auditNumber);
    
    // Verify results
    EXPECT_EQ(receipts.size(), 1);
    EXPECT_EQ(receipts[0]->auditNumber(), auditNumber);
    EXPECT_TRUE(receipts[0]->keyHash() != nullptr);
    EXPECT_TRUE(receipts[0]->signature() != nullptr);
    
    // Verify audit amounts map
    auto auditAmounts = mHandler->auditAmounts(trustLineID, auditNumber);
    EXPECT_EQ(auditAmounts.size(), 1);
    
    // Verify count method
    auto count = mHandler->countReceiptsByNumber(trustLineID, auditNumber);
    EXPECT_EQ(count, 1);
}

// Test Constructor with null connection
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, Constructor_NullConnection_ThrowsException) {
    EXPECT_THROW(
        IncomingPaymentReceiptHandlerPostgreSQL(nullptr, "test_table", mLogger),
        ValueError
    );
}

// Test Constructor with empty table name
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, Constructor_EmptyTableName_ThrowsException) {
    EXPECT_THROW(
        IncomingPaymentReceiptHandlerPostgreSQL(mConnection, "", mLogger),
        ValueError
    );
}

// Test table creation and schema validation
TEST_F(IncomingPaymentReceiptHandlerPostgreSQLIntegrationTest, TableCreation_ValidatesSchemaCorrectly) {
    // Test that the table was created with correct schema
    std::string schemaQuery = "SELECT column_name, data_type FROM information_schema.columns WHERE table_name = '" + mTestTableName + "' ORDER BY ordinal_position";
    PGresult* result = PQexec(mConnection, schemaQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    
    // Verify we have the expected columns
    int columnCount = PQntuples(result);
    EXPECT_EQ(columnCount, 6); // trust_line_id, audit_number, transaction_uuid, contractor_public_key_hash, amount, contractor_signature
    
    // Verify some key columns exist
    std::vector<std::string> expectedColumns = {"trust_line_id", "audit_number", "transaction_uuid", "contractor_public_key_hash", "amount", "contractor_signature"};
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

 