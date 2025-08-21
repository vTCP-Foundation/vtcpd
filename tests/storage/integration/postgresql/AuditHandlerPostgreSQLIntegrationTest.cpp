#include "gtest/gtest.h"
#include "../../../../src/core/io/storage/postgresql/AuditHandlerPostgreSQL.h"
#include "../../../../src/core/logger/Logger.h"
#include "../../../../src/core/io/storage/record/audit/AuditRecord.h"
#include "../fixtures/DatabaseTestHelper.h"
#include "../fixtures/PostgreSQLTestFixtures.h"
#include <memory>
#include <vector>
#include <sstream>
#include <cstring>
#include <libpq-fe.h>
#include <sodium.h>


class AuditHandlerPostgreSQLIntegrationTest : public ::testing::Test {
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
        mTestTableName = "audit_test_" + std::to_string(testCounter++);
        
        // Create trust_lines table (required by foreign key constraint)
        createTrustLinesTable();
        
        // Create test trust line records
        insertTestTrustLines();
        
        // Create AuditHandlerPostgreSQL instance
        mHandler = std::make_unique<AuditHandlerPostgreSQL>(
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
    
    
    Signature::Shared createTestSignature(const std::string& testData) {
        auto privateKey = std::make_unique<PrivateKey>();
        Signature::Shared sig = std::make_shared<Signature>();
        sig->sign(*privateKey, reinterpret_cast<const byte_t*>(testData.c_str()), testData.length());
        return sig;
    }
    
    TrustLineAmount createTestAmount(long long value) {
        return TrustLineAmount(value);
    }
    
    TrustLineBalance createTestBalance(long long value) {
        return TrustLineBalance(value);
    }
    
    int getAuditCount(TrustLineID trustLineID) {
        std::string query = "SELECT COUNT(*) FROM " + mTestTableName + " WHERE trust_line_id = " + std::to_string(trustLineID);
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get audit count");
        }
        
        int count = std::stoi(PQgetvalue(result, 0, 0));
        PQclear(result);
        return count;
    }
    
    // Helper method to verify raw database data
    struct RawAuditData {
        int number;
        int trustLineId;
        std::string ourSignatureHex;
        std::string contractorSignatureHex;
        std::string balanceHex;
        std::string outgoingAmountHex;
        std::string incomingAmountHex;
    };
    
    std::vector<RawAuditData> getRawAuditData(TrustLineID trustLineID) {
        std::string query = "SELECT number, trust_line_id, "
                           "encode(our_signature, 'hex'), encode(contractor_signature, 'hex'), "
                           "encode(balance, 'hex'), encode(outgoing_amount, 'hex'), encode(incoming_amount, 'hex') "
                           "FROM " + mTestTableName + " WHERE trust_line_id = " + std::to_string(trustLineID) + " ORDER BY number";
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get raw audit data");
        }
        
        std::vector<RawAuditData> data;
        int rows = PQntuples(result);
        
        for (int i = 0; i < rows; ++i) {
            RawAuditData rawData;
            rawData.number = std::stoi(PQgetvalue(result, i, 0));
            rawData.trustLineId = std::stoi(PQgetvalue(result, i, 1));
            rawData.ourSignatureHex = PQgetvalue(result, i, 2);
            rawData.contractorSignatureHex = PQgetvalue(result, i, 3) ? PQgetvalue(result, i, 3) : "";
            rawData.balanceHex = PQgetvalue(result, i, 4);
            rawData.outgoingAmountHex = PQgetvalue(result, i, 5);
            rawData.incomingAmountHex = PQgetvalue(result, i, 6);
            data.push_back(rawData);
        }
        
        PQclear(result);
        return data;
    }
    
    void insertAuditViaSQL(AuditNumber number, TrustLineID trustLineID, 
                           const std::string& ourSignatureHex,
                           const std::string& contractorSignatureHex,
                           const std::string& balanceHex, const std::string& outgoingAmountHex,
                           const std::string& incomingAmountHex) {
        std::string query = "INSERT INTO " + mTestTableName + 
                           " (number, trust_line_id, our_signature, contractor_signature, "
                           "balance, outgoing_amount, incoming_amount) "
                           "VALUES (" + std::to_string(number) + ", " + std::to_string(trustLineID) + ", "
                           "decode('" + ourSignatureHex + "', 'hex'), ";
        
        if (!contractorSignatureHex.empty()) {
            query += "decode('" + contractorSignatureHex + "', 'hex'), ";
        } else {
            query += "NULL, ";
        }
        
        query += "decode('" + balanceHex + "', 'hex'), decode('" + outgoingAmountHex + "', 'hex'), "
                 "decode('" + incomingAmountHex + "', 'hex'))";
        
        DatabaseTestHelper::executeQuery(mConnection, query);
    }

protected:
    PGconn* mConnection;
    std::unique_ptr<AuditHandlerPostgreSQL> mHandler;
    Logger mLogger;
    std::string mTestTableName;
    static int testCounter;
};

// Initialize static counter
int AuditHandlerPostgreSQLIntegrationTest::testCounter = 0;

// Test saveFullAudit method
TEST_F(AuditHandlerPostgreSQLIntegrationTest, saveFullAudit_ValidData_SavesSuccessfully) {
    AuditNumber number = 1;
    TrustLineID trustLineID = getValidTrustLineID();
    auto ownSignature = createTestSignature("ownSig");
    auto contractorSignature = createTestSignature("contractorSig");
    auto incomingAmount = createTestAmount(1000);
    auto outgoingAmount = createTestAmount(2000);
    auto balance = createTestBalance(3000);
    
    // Test the method
    EXPECT_NO_THROW(
        mHandler->saveFullAudit(number, trustLineID, ownSignature, 
                               contractorSignature, 
                               incomingAmount, outgoingAmount, balance)
    );
    
    // Verify data was saved
    EXPECT_EQ(getAuditCount(trustLineID), 1);
    
    // Verify raw database data
    auto rawData = getRawAuditData(trustLineID);
    EXPECT_EQ(rawData.size(), 1);
    EXPECT_EQ(rawData[0].number, number);
    EXPECT_EQ(rawData[0].trustLineId, trustLineID);
    EXPECT_FALSE(rawData[0].ourSignatureHex.empty());
    EXPECT_FALSE(rawData[0].contractorSignatureHex.empty());
    EXPECT_FALSE(rawData[0].balanceHex.empty());
    EXPECT_FALSE(rawData[0].outgoingAmountHex.empty());
    EXPECT_FALSE(rawData[0].incomingAmountHex.empty());
}

// Test saveOwnAuditPart method
TEST_F(AuditHandlerPostgreSQLIntegrationTest, saveOwnAuditPart_ValidData_SavesSuccessfully) {
    AuditNumber number = 1;
    TrustLineID trustLineID = getValidTrustLineID();
    auto ownSignature = createTestSignature("ownSig");
    auto incomingAmount = createTestAmount(1000);
    auto outgoingAmount = createTestAmount(2000);
    auto balance = createTestBalance(3000);
    
    // Test the method
    EXPECT_NO_THROW(
        mHandler->saveOwnAuditPart(number, trustLineID, ownSignature, 
                                   incomingAmount, outgoingAmount, balance)
    );
    
    // Verify data was saved
    EXPECT_EQ(getAuditCount(trustLineID), 1);
    
    // Verify raw database data - contractor fields should be NULL
    auto rawData = getRawAuditData(trustLineID);
    EXPECT_EQ(rawData.size(), 1);
    EXPECT_EQ(rawData[0].number, number);
    EXPECT_EQ(rawData[0].trustLineId, trustLineID);
    EXPECT_FALSE(rawData[0].ourSignatureHex.empty());
    EXPECT_TRUE(rawData[0].contractorSignatureHex.empty()); // Should be NULL
}

// Test saveContractorAuditPart method
TEST_F(AuditHandlerPostgreSQLIntegrationTest, saveContractorAuditPart_ValidData_UpdatesSuccessfully) {
    // First insert own audit part
    AuditNumber number = 1;
    TrustLineID trustLineID = getValidTrustLineID();
    
    auto ownSignature = createTestSignature("ownSig");
    auto incomingAmount = createTestAmount(1000);
    auto outgoingAmount = createTestAmount(2000);
    auto balance = createTestBalance(3000);
    
    mHandler->saveOwnAuditPart(number, trustLineID, ownSignature, 
                               incomingAmount, outgoingAmount, balance);
    
    // Now add contractor part
    auto contractorSignature = createTestSignature("contractorSig");
    
    EXPECT_NO_THROW(
        mHandler->saveContractorAuditPart(number, trustLineID, contractorSignature)
    );
    
    // Verify data was updated
    auto rawData = getRawAuditData(trustLineID);
    EXPECT_EQ(rawData.size(), 1);
    EXPECT_FALSE(rawData[0].contractorSignatureHex.empty()); // Should now have data
}

// Test saveContractorAuditPart with non-existent audit
TEST_F(AuditHandlerPostgreSQLIntegrationTest, saveContractorAuditPart_NonExistentAudit_ThrowsException) {
    AuditNumber number = 999;
    TrustLineID trustLineID = getValidTrustLineID();
    
    auto contractorSignature = createTestSignature("contractorSig");
    
    EXPECT_THROW(
        mHandler->saveContractorAuditPart(number, trustLineID, contractorSignature),
        ValueError
    );
}

// Test getActualAudit method
TEST_F(AuditHandlerPostgreSQLIntegrationTest, getActualAudit_ValidData_ReturnsCorrectAudit) {
    // Insert test data
    AuditNumber number1 = 1;
    AuditNumber number2 = 2;
    TrustLineID trustLineID = getValidTrustLineID();
    
    auto ownSignature = createTestSignature("ownSig");
    auto incomingAmount = createTestAmount(1000);
    auto outgoingAmount = createTestAmount(2000);
    auto balance = createTestBalance(3000);
    
    // Insert two audits
    mHandler->saveOwnAuditPart(number1, trustLineID, ownSignature, 
                               incomingAmount, outgoingAmount, balance);
    mHandler->saveOwnAuditPart(number2, trustLineID, ownSignature, 
                               incomingAmount, outgoingAmount, balance);
    
    // Test the method - should return the latest audit
    auto actualAudit = mHandler->getActualAudit(trustLineID);
    EXPECT_EQ(actualAudit->auditNumber(), number2);
    EXPECT_EQ(actualAudit->incomingAmount(), incomingAmount);
    EXPECT_EQ(actualAudit->outgoingAmount(), outgoingAmount);
    EXPECT_EQ(actualAudit->balance(), balance);
}

// Test getActualAudit with non-existent trust line
TEST_F(AuditHandlerPostgreSQLIntegrationTest, getActualAudit_NonExistentTrustLine_ThrowsException) {
    TrustLineID nonExistentTrustLineID = 999;
    
    EXPECT_THROW(
        mHandler->getActualAudit(nonExistentTrustLineID),
        NotFoundError
    );
}

// Test getActualAuditFull method
TEST_F(AuditHandlerPostgreSQLIntegrationTest, getActualAuditFull_ValidData_ReturnsFullAudit) {
    // Insert full audit
    AuditNumber number = 1;
    TrustLineID trustLineID = getValidTrustLineID();
    
    auto ownSignature = createTestSignature("ownSig");
    auto contractorSignature = createTestSignature("contractorSig");
    auto incomingAmount = createTestAmount(1000);
    auto outgoingAmount = createTestAmount(2000);
    auto balance = createTestBalance(3000);
    
    mHandler->saveFullAudit(number, trustLineID, ownSignature, 
                           contractorSignature, 
                           incomingAmount, outgoingAmount, balance);
    
    // Test the method
    auto actualAudit = mHandler->getActualAuditFull(trustLineID);
    EXPECT_EQ(actualAudit->auditNumber(), number);
    EXPECT_EQ(actualAudit->incomingAmount(), incomingAmount);
    EXPECT_EQ(actualAudit->outgoingAmount(), outgoingAmount);
    EXPECT_EQ(actualAudit->balance(), balance);
    EXPECT_TRUE(actualAudit->ownSignature() != nullptr);
    EXPECT_TRUE(actualAudit->contractorSignature() != nullptr);
}

// Test getActualAuditNumber method
TEST_F(AuditHandlerPostgreSQLIntegrationTest, getActualAuditNumber_ValidData_ReturnsCorrectNumber) {
    // Insert test data
    AuditNumber number1 = 1;
    AuditNumber number2 = 2;
    TrustLineID trustLineID = getValidTrustLineID();
    
    auto ownSignature = createTestSignature("ownSig");
    auto incomingAmount = createTestAmount(1000);
    auto outgoingAmount = createTestAmount(2000);
    auto balance = createTestBalance(3000);
    
    // Insert two audits
    mHandler->saveOwnAuditPart(number1, trustLineID, ownSignature, 
                               incomingAmount, outgoingAmount, balance);
    mHandler->saveOwnAuditPart(number2, trustLineID, ownSignature, 
                               incomingAmount, outgoingAmount, balance);
    
    // Test the method - should return the latest audit number
    auto actualNumber = mHandler->getActualAuditNumber(trustLineID);
    EXPECT_EQ(actualNumber, number2);
}

// Test getActualAuditNumber with non-existent trust line
TEST_F(AuditHandlerPostgreSQLIntegrationTest, getActualAuditNumber_NonExistentTrustLine_ThrowsException) {
    TrustLineID nonExistentTrustLineID = 999;
    
    EXPECT_THROW(
        mHandler->getActualAuditNumber(nonExistentTrustLineID),
        NotFoundError
    );
}

// Test deleteRecords method
TEST_F(AuditHandlerPostgreSQLIntegrationTest, deleteRecords_ValidData_DeletesSuccessfully) {
    // Insert test data
    AuditNumber number = 1;
    TrustLineID trustLineID = getValidTrustLineID();
    
    auto ownSignature = createTestSignature("ownSig");
    auto incomingAmount = createTestAmount(1000);
    auto outgoingAmount = createTestAmount(2000);
    auto balance = createTestBalance(3000);
    
    mHandler->saveOwnAuditPart(number, trustLineID, ownSignature, 
                               incomingAmount, outgoingAmount, balance);
    
    // Verify data exists
    EXPECT_EQ(getAuditCount(trustLineID), 1);
    
    // Test the method
    EXPECT_NO_THROW(mHandler->deleteRecords(trustLineID));
    
    // Verify data was deleted
    EXPECT_EQ(getAuditCount(trustLineID), 0);
}

// Test deleteAuditByNumber method
TEST_F(AuditHandlerPostgreSQLIntegrationTest, deleteAuditByNumber_ValidData_DeletesSuccessfully) {
    // Insert test data
    AuditNumber number1 = 1;
    AuditNumber number2 = 2;
    TrustLineID trustLineID = getValidTrustLineID();
    
    auto ownSignature = createTestSignature("ownSig");
    auto incomingAmount = createTestAmount(1000);
    auto outgoingAmount = createTestAmount(2000);
    auto balance = createTestBalance(3000);
    
    // Insert two audits
    mHandler->saveOwnAuditPart(number1, trustLineID, ownSignature, 
                               incomingAmount, outgoingAmount, balance);
    mHandler->saveOwnAuditPart(number2, trustLineID, ownSignature, 
                               incomingAmount, outgoingAmount, balance);
    
    // Verify data exists
    EXPECT_EQ(getAuditCount(trustLineID), 2);
    
    // Test the method - delete first audit
    EXPECT_NO_THROW(mHandler->deleteAuditByNumber(trustLineID, number1));
    
    // Verify only one audit remains
    EXPECT_EQ(getAuditCount(trustLineID), 1);
    
    // Verify the remaining audit is the second one
    auto actualNumber = mHandler->getActualAuditNumber(trustLineID);
    EXPECT_EQ(actualNumber, number2);
}

// Test deleteAuditByNumber with non-existent audit
TEST_F(AuditHandlerPostgreSQLIntegrationTest, deleteAuditByNumber_NonExistentAudit_ThrowsException) {
    AuditNumber nonExistentNumber = 999;
    TrustLineID trustLineID = getValidTrustLineID();
    
    EXPECT_THROW(
        mHandler->deleteAuditByNumber(trustLineID, nonExistentNumber),
        ValueError
    );
}

// Test auditsLessEqualThanAuditNumber method
TEST_F(AuditHandlerPostgreSQLIntegrationTest, auditsLessEqualThanAuditNumber_ValidData_ReturnsCorrectAudits) {
    // Insert test data
    TrustLineID trustLineID = getValidTrustLineID();
    
    auto ownSignature = createTestSignature("ownSig");
    auto contractorSignature = createTestSignature("contractorSig");
    auto incomingAmount = createTestAmount(1000);
    auto outgoingAmount = createTestAmount(2000);
    auto balance = createTestBalance(3000);
    
    // Insert multiple audits
    for (AuditNumber i = 1; i <= 5; ++i) {
        mHandler->saveFullAudit(i, trustLineID, ownSignature, 
                               contractorSignature, 
                               incomingAmount, outgoingAmount, balance);
    }
    
    // Test the method
    auto audits = mHandler->auditsLessEqualThanAuditNumber(trustLineID, 3);
    EXPECT_EQ(audits.size(), 3);
    
    // Verify audit numbers
    for (size_t i = 0; i < audits.size(); ++i) {
        EXPECT_EQ(audits[i]->auditNumber(), i + 1);
    }
}

// Test auditsLessEqualThanAuditNumber with no matching audits
TEST_F(AuditHandlerPostgreSQLIntegrationTest, auditsLessEqualThanAuditNumber_NoMatchingAudits_ReturnsEmpty) {
    TrustLineID trustLineID = getValidTrustLineID();
    
    // Test with no audits
    auto audits = mHandler->auditsLessEqualThanAuditNumber(trustLineID, 3);
    EXPECT_EQ(audits.size(), 0);
}



// Test Raw Database Data Validation
TEST_F(AuditHandlerPostgreSQLIntegrationTest, RawDataValidation_SaveFullAudit_CorrectDatabaseStorage) {
    AuditNumber number = 1;
    TrustLineID trustLineID = getValidTrustLineID();
    
    auto ownSignature = createTestSignature("ownSig");
    auto contractorSignature = createTestSignature("contractorSig");
    auto incomingAmount = createTestAmount(1000);
    auto outgoingAmount = createTestAmount(2000);
    auto balance = createTestBalance(3000);
    
    mHandler->saveFullAudit(number, trustLineID, ownSignature, 
                           contractorSignature, 
                           incomingAmount, outgoingAmount, balance);
    
    // Verify raw database data using direct SQL queries
    std::string countQuery = "SELECT COUNT(*) FROM " + mTestTableName + " WHERE trust_line_id = " + std::to_string(trustLineID);
    PGresult* result = PQexec(mConnection, countQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    EXPECT_EQ(std::stoi(PQgetvalue(result, 0, 0)), 1);
    PQclear(result);
    
    // Verify specific field values
    std::string dataQuery = "SELECT number, trust_line_id FROM " + mTestTableName + " WHERE trust_line_id = " + std::to_string(trustLineID);
    result = PQexec(mConnection, dataQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    EXPECT_EQ(std::stoi(PQgetvalue(result, 0, 0)), number);
    EXPECT_EQ(std::stoi(PQgetvalue(result, 0, 1)), trustLineID);
    PQclear(result);
}

// Test Reverse Validation: Insert via SQL, read via class methods
TEST_F(AuditHandlerPostgreSQLIntegrationTest, ReverseValidation_InsertViaSQL_ReadViaClass) {
    AuditNumber number = 1;
    TrustLineID trustLineID = getValidTrustLineID();
    
    // Generate test data as hex strings
    std::string ownSignatureHex = "0000000000000000000000000000000000000000000000000000000000000002";
    std::string contractorSignatureHex = "0000000000000000000000000000000000000000000000000000000000000004";
    std::string balanceHex = "00000000000000000000000000000bb8"; // 3000 in balance format
    std::string outgoingAmountHex = "000000000000000000000000000007d0"; // 2000 in amount format
    std::string incomingAmountHex = "00000000000000000000000000000000000000000000000000000000000003e8"; // 1000 in amount format
    
    // Insert data via SQL
    insertAuditViaSQL(number, trustLineID, ownSignatureHex, contractorSignatureHex, 
                     balanceHex, outgoingAmountHex, incomingAmountHex);
    
    // Read data via class methods
    auto actualAudit = mHandler->getActualAuditFull(trustLineID);
    EXPECT_EQ(actualAudit->auditNumber(), number);
    EXPECT_TRUE(actualAudit->ownSignature() != nullptr);
    EXPECT_TRUE(actualAudit->contractorSignature() != nullptr);
}

// Test Constructor with null connection
TEST_F(AuditHandlerPostgreSQLIntegrationTest, Constructor_NullConnection_ThrowsException) {
    EXPECT_THROW(
        AuditHandlerPostgreSQL(nullptr, "test_table", mLogger),
        IOError
    );
}

// Test table creation and schema validation
TEST_F(AuditHandlerPostgreSQLIntegrationTest, TableCreation_ValidatesSchemaCorrectly) {
    // Test that the table was created with correct schema
    std::string schemaQuery = "SELECT column_name, data_type FROM information_schema.columns WHERE table_name = '" + mTestTableName + "' ORDER BY ordinal_position";
    PGresult* result = PQexec(mConnection, schemaQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    
    // Verify we have the expected columns
    int columnCount = PQntuples(result);
    EXPECT_EQ(columnCount, 7); // number, trust_line_id, our_signature, contractor_signature, balance, outgoing_amount, incoming_amount
    
    // Verify some key columns exist
    std::vector<std::string> expectedColumns = {"number", "trust_line_id", "our_signature", "balance"};
    std::vector<std::string> actualColumns;
    for (int i = 0; i < columnCount; ++i) {
        actualColumns.push_back(PQgetvalue(result, i, 0));
    }
    
    for (const auto& expectedCol : expectedColumns) {
        EXPECT_TRUE(std::find(actualColumns.begin(), actualColumns.end(), expectedCol) != actualColumns.end());
    }
    
    PQclear(result);
} 