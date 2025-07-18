#include "gtest/gtest.h"
#include "../../../../src/core/io/storage/postgresql/AuditRulesHandlerPostgreSQL.h"
#include "../../../../src/core/logger/Logger.h"
#include "../../../../src/core/trust_lines/audit_rules/BaseAuditRule.h"
#include "../fixtures/DatabaseTestHelper.h"
#include "../fixtures/PostgreSQLTestFixtures.h"
#include <memory>
#include <vector>
#include <sstream>
#include <libpq-fe.h>

class AuditRulesHandlerPostgreSQLIntegrationTest : public ::testing::Test {
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
        mTestTableName = "audit_rules_test_" + std::to_string(testCounter++);
        
        // Create trust_lines table (required by foreign key constraint)
        createTrustLinesTable();
        
        // Create test trust line records
        insertTestTrustLines();
        
        // Create AuditRulesHandlerPostgreSQL instance
        mHandler = std::make_unique<AuditRulesHandlerPostgreSQL>(
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
    
    BaseAuditRule::AuditRuleType getTestAuditRuleType1() const {
        return BaseAuditRule::AuditRuleCountPaymentsType;
    }
    
    BaseAuditRule::AuditRuleType getTestAuditRuleType2() const {
        return BaseAuditRule::AuditRuleTimeType;
    }
    
    BaseAuditRule::AuditRuleType getTestAuditRuleType3() const {
        return BaseAuditRule::AuditRuleTrustLineAmountBoundaryType;
    }
    
    int getRuleCount(TrustLineID trustLineID) {
        std::string query = "SELECT COUNT(*) FROM " + mTestTableName + " WHERE trust_line_id = " + std::to_string(trustLineID);
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get rule count");
        }
        
        int count = std::stoi(PQgetvalue(result, 0, 0));
        PQclear(result);
        return count;
    }
    
    // Helper method to verify raw database data
    struct RawRuleData {
        int trustLineId;
        int ruleId;
    };
    
    std::vector<RawRuleData> getRawRuleData(TrustLineID trustLineID) {
        std::string query = "SELECT trust_line_id, rule_id FROM " + mTestTableName + 
                           " WHERE trust_line_id = " + std::to_string(trustLineID) + " ORDER BY rule_id";
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get raw rule data");
        }
        
        std::vector<RawRuleData> data;
        int rows = PQntuples(result);
        
        for (int i = 0; i < rows; ++i) {
            RawRuleData rawData;
            rawData.trustLineId = std::stoi(PQgetvalue(result, i, 0));
            rawData.ruleId = std::stoi(PQgetvalue(result, i, 1));
            data.push_back(rawData);
        }
        
        PQclear(result);
        return data;
    }
    
    void insertRuleViaSQL(TrustLineID trustLineID, BaseAuditRule::AuditRuleType ruleType) {
        std::string query = "INSERT INTO " + mTestTableName + " (trust_line_id, rule_id) VALUES (" 
                           + std::to_string(trustLineID) + ", " + std::to_string(static_cast<int>(ruleType)) + ")";
        DatabaseTestHelper::executeQuery(mConnection, query);
    }

protected:
    PGconn* mConnection;
    std::unique_ptr<AuditRulesHandlerPostgreSQL> mHandler;
    Logger mLogger;
    std::string mTestTableName;
    static int testCounter;
};

// Initialize static counter
int AuditRulesHandlerPostgreSQLIntegrationTest::testCounter = 0;

// Test saveRule method
TEST_F(AuditRulesHandlerPostgreSQLIntegrationTest, saveRule_ValidData_SavesSuccessfully) {
    TrustLineID trustLineID = getValidTrustLineID();
    BaseAuditRule::AuditRuleType ruleType = getTestAuditRuleType1();
    
    // Test the method
    EXPECT_NO_THROW(mHandler->saveRule(trustLineID, ruleType));
    
    // Verify data was saved
    EXPECT_EQ(getRuleCount(trustLineID), 1);
    
    // Verify raw database data
    auto rawData = getRawRuleData(trustLineID);
    EXPECT_EQ(rawData.size(), 1);
    EXPECT_EQ(rawData[0].trustLineId, trustLineID);
    EXPECT_EQ(rawData[0].ruleId, static_cast<int>(ruleType));
}

// Test saveRule with duplicate trust line (should fail due to unique constraint)
TEST_F(AuditRulesHandlerPostgreSQLIntegrationTest, saveRule_DuplicateTrustLine_ThrowsException) {
    TrustLineID trustLineID = getValidTrustLineID();
    BaseAuditRule::AuditRuleType ruleType1 = getTestAuditRuleType1();
    BaseAuditRule::AuditRuleType ruleType2 = getTestAuditRuleType2();
    
    // Save first rule
    EXPECT_NO_THROW(mHandler->saveRule(trustLineID, ruleType1));
    EXPECT_EQ(getRuleCount(trustLineID), 1);
    
    // Try to save second rule for same trust line (should fail due to unique constraint)
    EXPECT_THROW(mHandler->saveRule(trustLineID, ruleType2), IOError);
    
    // Verify only first rule remains
    EXPECT_EQ(getRuleCount(trustLineID), 1);
    auto rawData = getRawRuleData(trustLineID);
    EXPECT_EQ(rawData.size(), 1);
    EXPECT_EQ(rawData[0].ruleId, static_cast<int>(ruleType1));
}

// Test saveRule with different trust lines
TEST_F(AuditRulesHandlerPostgreSQLIntegrationTest, saveRule_DifferentTrustLines_SavesSuccessfully) {
    TrustLineID trustLineID1 = getValidTrustLineID();
    TrustLineID trustLineID2 = getValidTrustLineID2();
    BaseAuditRule::AuditRuleType ruleType = getTestAuditRuleType1();
    
    // Save rules for different trust lines
    EXPECT_NO_THROW(mHandler->saveRule(trustLineID1, ruleType));
    EXPECT_NO_THROW(mHandler->saveRule(trustLineID2, ruleType));
    
    // Verify data was saved for both trust lines
    EXPECT_EQ(getRuleCount(trustLineID1), 1);
    EXPECT_EQ(getRuleCount(trustLineID2), 1);
}

// Test saveRule with all rule types (use different trust lines)
TEST_F(AuditRulesHandlerPostgreSQLIntegrationTest, saveRule_AllRuleTypes_SavesSuccessfully) {
    TrustLineID trustLineID1 = getValidTrustLineID();
    TrustLineID trustLineID2 = getValidTrustLineID2();
    TrustLineID trustLineID3 = getValidTrustLineID3();
    
    // Test all rule types using different trust lines (due to unique constraint)
    EXPECT_NO_THROW(mHandler->saveRule(trustLineID1, BaseAuditRule::AuditRuleCountPaymentsType));
    EXPECT_NO_THROW(mHandler->saveRule(trustLineID2, BaseAuditRule::AuditRuleTimeType));
    EXPECT_NO_THROW(mHandler->saveRule(trustLineID3, BaseAuditRule::AuditRuleTrustLineAmountBoundaryType));
    
    // Verify each trust line has one rule
    EXPECT_EQ(getRuleCount(trustLineID1), 1);
    EXPECT_EQ(getRuleCount(trustLineID2), 1);
    EXPECT_EQ(getRuleCount(trustLineID3), 1);
    
    // Verify rule types are saved correctly
    auto rawData1 = getRawRuleData(trustLineID1);
    auto rawData2 = getRawRuleData(trustLineID2);
    auto rawData3 = getRawRuleData(trustLineID3);
    
    EXPECT_EQ(rawData1.size(), 1);
    EXPECT_EQ(rawData2.size(), 1);
    EXPECT_EQ(rawData3.size(), 1);
    
    EXPECT_EQ(rawData1[0].ruleId, static_cast<int>(BaseAuditRule::AuditRuleCountPaymentsType));
    EXPECT_EQ(rawData2[0].ruleId, static_cast<int>(BaseAuditRule::AuditRuleTimeType));
    EXPECT_EQ(rawData3[0].ruleId, static_cast<int>(BaseAuditRule::AuditRuleTrustLineAmountBoundaryType));
}

// Test getRule method
TEST_F(AuditRulesHandlerPostgreSQLIntegrationTest, getRule_ValidData_ReturnsCorrectRule) {
    TrustLineID trustLineID = getValidTrustLineID();
    BaseAuditRule::AuditRuleType expectedRuleType = getTestAuditRuleType2();
    
    // First save a rule
    mHandler->saveRule(trustLineID, expectedRuleType);
    
    // Test the method
    BaseAuditRule::AuditRuleType actualRuleType = mHandler->getRule(trustLineID);
    EXPECT_EQ(actualRuleType, expectedRuleType);
}

// Test getRule after failed duplicate insert
TEST_F(AuditRulesHandlerPostgreSQLIntegrationTest, getRule_AfterFailedDuplicateInsert_ReturnsOriginalRule) {
    TrustLineID trustLineID = getValidTrustLineID();
    BaseAuditRule::AuditRuleType firstRuleType = getTestAuditRuleType1();
    BaseAuditRule::AuditRuleType secondRuleType = getTestAuditRuleType2();
    
    // Save first rule
    mHandler->saveRule(trustLineID, firstRuleType);
    
    // Try to save second rule (should fail)
    EXPECT_THROW(mHandler->saveRule(trustLineID, secondRuleType), IOError);
    
    // Test the method - should return the first rule
    BaseAuditRule::AuditRuleType actualRuleType = mHandler->getRule(trustLineID);
    EXPECT_EQ(actualRuleType, firstRuleType);
}

// Test getRule with non-existent trust line
TEST_F(AuditRulesHandlerPostgreSQLIntegrationTest, getRule_NonExistentTrustLine_ThrowsException) {
    TrustLineID nonExistentTrustLineID = 999;
    
    EXPECT_THROW(
        mHandler->getRule(nonExistentTrustLineID),
        NotFoundError
    );
}

// Test getRule with all rule types
TEST_F(AuditRulesHandlerPostgreSQLIntegrationTest, getRule_AllRuleTypes_ReturnsCorrectTypes) {
    TrustLineID trustLineID1 = getValidTrustLineID();
    TrustLineID trustLineID2 = getValidTrustLineID2();
    TrustLineID trustLineID3 = getValidTrustLineID3();
    
    // Save different rule types for different trust lines
    mHandler->saveRule(trustLineID1, BaseAuditRule::AuditRuleCountPaymentsType);
    mHandler->saveRule(trustLineID2, BaseAuditRule::AuditRuleTimeType);
    mHandler->saveRule(trustLineID3, BaseAuditRule::AuditRuleTrustLineAmountBoundaryType);
    
    // Test getting each rule type
    EXPECT_EQ(mHandler->getRule(trustLineID1), BaseAuditRule::AuditRuleCountPaymentsType);
    EXPECT_EQ(mHandler->getRule(trustLineID2), BaseAuditRule::AuditRuleTimeType);
    EXPECT_EQ(mHandler->getRule(trustLineID3), BaseAuditRule::AuditRuleTrustLineAmountBoundaryType);
}

// Test removeAuditRules method
TEST_F(AuditRulesHandlerPostgreSQLIntegrationTest, removeAuditRules_ValidData_RemovesSuccessfully) {
    TrustLineID trustLineID = getValidTrustLineID();
    BaseAuditRule::AuditRuleType ruleType = getTestAuditRuleType1();
    
    // First save a rule
    mHandler->saveRule(trustLineID, ruleType);
    EXPECT_EQ(getRuleCount(trustLineID), 1);
    
    // Test the method
    EXPECT_NO_THROW(mHandler->removeAuditRules(trustLineID));
    
    // Verify data was removed
    EXPECT_EQ(getRuleCount(trustLineID), 0);
}

// Test removeAuditRules after rule update
TEST_F(AuditRulesHandlerPostgreSQLIntegrationTest, removeAuditRules_AfterRuleUpdate_RemovesSuccessfully) {
    TrustLineID trustLineID = getValidTrustLineID();
    
    // Save first rule
    mHandler->saveRule(trustLineID, getTestAuditRuleType1());
    EXPECT_EQ(getRuleCount(trustLineID), 1);
    
    // Test removal
    EXPECT_NO_THROW(mHandler->removeAuditRules(trustLineID));
    
    // Verify data was removed
    EXPECT_EQ(getRuleCount(trustLineID), 0);
    
    // Should be able to add a different rule now
    EXPECT_NO_THROW(mHandler->saveRule(trustLineID, getTestAuditRuleType2()));
    EXPECT_EQ(getRuleCount(trustLineID), 1);
}

// Test removeAuditRules with non-existent trust line
TEST_F(AuditRulesHandlerPostgreSQLIntegrationTest, removeAuditRules_NonExistentTrustLine_DoesNotThrow) {
    TrustLineID nonExistentTrustLineID = 999;
    
    // Should not throw even if trust line doesn't exist
    EXPECT_NO_THROW(mHandler->removeAuditRules(nonExistentTrustLineID));
}

// Test removeAuditRules isolation between trust lines
TEST_F(AuditRulesHandlerPostgreSQLIntegrationTest, removeAuditRules_IsolatesBetweenTrustLines) {
    TrustLineID trustLineID1 = getValidTrustLineID();
    TrustLineID trustLineID2 = getValidTrustLineID2();
    BaseAuditRule::AuditRuleType ruleType = getTestAuditRuleType1();
    
    // Save rules for both trust lines
    mHandler->saveRule(trustLineID1, ruleType);
    mHandler->saveRule(trustLineID2, ruleType);
    EXPECT_EQ(getRuleCount(trustLineID1), 1);
    EXPECT_EQ(getRuleCount(trustLineID2), 1);
    
    // Remove rules for first trust line only
    mHandler->removeAuditRules(trustLineID1);
    
    // Verify only first trust line's rules are removed
    EXPECT_EQ(getRuleCount(trustLineID1), 0);
    EXPECT_EQ(getRuleCount(trustLineID2), 1);
}

// Test Raw Database Data Validation
TEST_F(AuditRulesHandlerPostgreSQLIntegrationTest, RawDataValidation_SaveRule_CorrectDatabaseStorage) {
    TrustLineID trustLineID = getValidTrustLineID();
    BaseAuditRule::AuditRuleType ruleType = getTestAuditRuleType2();
    
    mHandler->saveRule(trustLineID, ruleType);
    
    // Verify raw database data using direct SQL queries
    std::string countQuery = "SELECT COUNT(*) FROM " + mTestTableName + " WHERE trust_line_id = " + std::to_string(trustLineID);
    PGresult* result = PQexec(mConnection, countQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    EXPECT_EQ(std::stoi(PQgetvalue(result, 0, 0)), 1);
    PQclear(result);
    
    // Verify specific field values
    std::string dataQuery = "SELECT trust_line_id, rule_id FROM " + mTestTableName + " WHERE trust_line_id = " + std::to_string(trustLineID);
    result = PQexec(mConnection, dataQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    EXPECT_EQ(std::stoi(PQgetvalue(result, 0, 0)), trustLineID);
    EXPECT_EQ(std::stoi(PQgetvalue(result, 0, 1)), static_cast<int>(ruleType));
    PQclear(result);
}

// Test Reverse Validation: Insert via SQL, read via class methods
TEST_F(AuditRulesHandlerPostgreSQLIntegrationTest, ReverseValidation_InsertViaSQL_ReadViaClass) {
    TrustLineID trustLineID = getValidTrustLineID();
    BaseAuditRule::AuditRuleType ruleType = getTestAuditRuleType3();
    
    // Insert data via SQL
    insertRuleViaSQL(trustLineID, ruleType);
    
    // Read data via class methods
    BaseAuditRule::AuditRuleType actualRuleType = mHandler->getRule(trustLineID);
    EXPECT_EQ(actualRuleType, ruleType);
}

// Test Complete Workflow
TEST_F(AuditRulesHandlerPostgreSQLIntegrationTest, CompleteWorkflow_SaveGetRemove_WorksCorrectly) {
    TrustLineID trustLineID = getValidTrustLineID();
    BaseAuditRule::AuditRuleType ruleType = getTestAuditRuleType1();
    
    // Save rule
    EXPECT_NO_THROW(mHandler->saveRule(trustLineID, ruleType));
    EXPECT_EQ(getRuleCount(trustLineID), 1);
    
    // Get rule
    BaseAuditRule::AuditRuleType retrievedRuleType = mHandler->getRule(trustLineID);
    EXPECT_EQ(retrievedRuleType, ruleType);
    
    // Remove rule
    EXPECT_NO_THROW(mHandler->removeAuditRules(trustLineID));
    EXPECT_EQ(getRuleCount(trustLineID), 0);
    
    // Verify rule is no longer available
    EXPECT_THROW(mHandler->getRule(trustLineID), NotFoundError);
}

// Test Constructor with null connection
TEST_F(AuditRulesHandlerPostgreSQLIntegrationTest, Constructor_NullConnection_ThrowsException) {
    EXPECT_THROW(
        AuditRulesHandlerPostgreSQL(nullptr, "test_table", mLogger),
        IOError
    );
}

// Test table creation and schema validation
TEST_F(AuditRulesHandlerPostgreSQLIntegrationTest, TableCreation_ValidatesSchemaCorrectly) {
    // Test that the table was created with correct schema
    std::string schemaQuery = "SELECT column_name, data_type FROM information_schema.columns WHERE table_name = '" + mTestTableName + "' ORDER BY ordinal_position";
    PGresult* result = PQexec(mConnection, schemaQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    
    // Verify we have the expected columns
    int columnCount = PQntuples(result);
    EXPECT_EQ(columnCount, 3); // trust_line_id, rule_id, parameters
    
    // Verify some key columns exist
    std::vector<std::string> expectedColumns = {"trust_line_id", "rule_id", "parameters"};
    std::vector<std::string> actualColumns;
    for (int i = 0; i < columnCount; ++i) {
        actualColumns.push_back(PQgetvalue(result, i, 0));
    }
    
    for (const auto& expectedCol : expectedColumns) {
        EXPECT_TRUE(std::find(actualColumns.begin(), actualColumns.end(), expectedCol) != actualColumns.end());
    }
    
    PQclear(result);
}

// Test index creation
TEST_F(AuditRulesHandlerPostgreSQLIntegrationTest, IndexCreation_ValidatesIndexExists) {
    // Check if unique index was created
    std::string indexQuery = "SELECT indexname FROM pg_indexes WHERE tablename = '" + mTestTableName + "' AND indexname LIKE '%trust_line_id_idx'";
    PGresult* result = PQexec(mConnection, indexQuery.c_str());
    EXPECT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    EXPECT_EQ(PQntuples(result), 1); // Should have one index
    PQclear(result);
} 