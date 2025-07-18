#include "../fixtures/DatabaseTestHelper.h"
#include "../fixtures/PostgreSQLTestFixtures.h"
#include "../../../../src/core/io/storage/postgresql/TrustLineHandlerPostgreSQL.h"
#include "../../../../src/core/logger/Logger.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <sstream>
#include <libpq-fe.h>

class TrustLineHandlerPostgreSQLTest : public ::testing::Test 
{
protected:
    void SetUp() override 
    {
        // Create database connection
        dbConnection = DatabaseTestHelper::createConnection(
            DatabaseTestHelper::TEST_HOST,
            DatabaseTestHelper::TEST_PORT,
            DatabaseTestHelper::TEST_USER,
            DatabaseTestHelper::TEST_PASSWORD,
            DatabaseTestHelper::TEST_DB_NAME
        );
        
        // Create unique table name for each test
        tableName = "trust_lines_test_" + std::to_string(testCounter++);
        
        // Logger setup
        logger = std::make_unique<Logger>();
        
        // Create handler
        handler = std::make_unique<TrustLineHandlerPostgreSQL>(
            dbConnection, 
            tableName, 
            *logger
        );
        
        // Create contractors table (required by foreign key constraint)
        createContractorsTable();
        
        // Create test contractor records
        insertTestContractors();
    }
    
    void TearDown() override 
    {
        // Clean up test data
        cleanupTestData();
        
        // Close database connection
        DatabaseTestHelper::closeConnection(dbConnection);
        
        handler.reset();
        logger.reset();
    }
    
    void createContractorsTable() {
        std::string query = "CREATE TABLE IF NOT EXISTS contractors ("
                           "id INTEGER PRIMARY KEY, "
                           "id_on_contractor_side INTEGER, "
                           "crypto_key BYTEA NOT NULL, "
                           "is_confirmed INTEGER NOT NULL DEFAULT 0)";
        DatabaseTestHelper::executeQuery(dbConnection, query);
    }
    
    void insertTestContractors() {
        std::vector<ContractorID> contractorIDs = {
            PostgreSQLTestFixtures::getValidContractorID(),
            PostgreSQLTestFixtures::getValidContractorID2(),
            PostgreSQLTestFixtures::getValidContractorID3()
        };
        
        for (const auto& contractorID : contractorIDs) {
            std::string query = "INSERT INTO contractors (id, crypto_key, is_confirmed) VALUES (" 
                               + std::to_string(contractorID) + ", '\\x" + std::string(64, '0') + "', 1) ON CONFLICT (id) DO NOTHING";
            DatabaseTestHelper::executeQuery(dbConnection, query);
        }
    }
    
    void cleanupTestData() {
        try {
            DatabaseTestHelper::cleanupTable(dbConnection, tableName);
            DatabaseTestHelper::cleanupTable(dbConnection, "contractors");
        } catch (const std::exception& e) {
            // Continue cleanup even if some operations fail
            std::cerr << "Cleanup warning: " << e.what() << std::endl;
        }
    }
    
    // Helper methods for database validation
    bool trustLineExistsInDatabase(TrustLineID id, ContractorID contractorID, SerializedEquivalent equivalent) 
    {
        std::string query = "SELECT COUNT(*) FROM " + tableName + 
                          " WHERE id = $1 AND contractor_id = $2 AND equivalent = $3;";
        
        std::string idStr = std::to_string(id);
        std::string contractorStr = std::to_string(contractorID);
        std::string equivalentStr = std::to_string(equivalent);
        
        const char *params[] = {idStr.c_str(), contractorStr.c_str(), equivalentStr.c_str()};
        int lengths[] = {0, 0, 0};
        int formats[] = {0, 0, 0};
        
        PGresult *result = PQexecParams(dbConnection, query.c_str(), 3, nullptr, params, lengths, formats, 0);
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            return false;
        }
        
        int count = atoi(PQgetvalue(result, 0, 0));
        PQclear(result);
        
        return count > 0;
    }
    
    struct DatabaseTrustLineData {
        TrustLineID id;
        int state;
        ContractorID contractorID;
        SerializedEquivalent equivalent;
        int isContractorGateway;
    };
    
    DatabaseTrustLineData getRawTrustLineData(TrustLineID id, ContractorID contractorID, SerializedEquivalent equivalent) 
    {
        std::string query = "SELECT id, state, contractor_id, equivalent, is_contractor_gateway FROM " + tableName + 
                          " WHERE id = $1 AND contractor_id = $2 AND equivalent = $3;";
        
        std::string idStr = std::to_string(id);
        std::string contractorStr = std::to_string(contractorID);
        std::string equivalentStr = std::to_string(equivalent);
        
        const char *params[] = {idStr.c_str(), contractorStr.c_str(), equivalentStr.c_str()};
        int lengths[] = {0, 0, 0};
        int formats[] = {0, 0, 0};
        
        PGresult *result = PQexecParams(dbConnection, query.c_str(), 3, nullptr, params, lengths, formats, 0);
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
            PQclear(result);
            throw std::runtime_error("Failed to fetch raw trust line data");
        }
        
        DatabaseTrustLineData data;
        data.id = static_cast<TrustLineID>(atoi(PQgetvalue(result, 0, 0)));
        data.state = atoi(PQgetvalue(result, 0, 1));
        data.contractorID = static_cast<ContractorID>(atoi(PQgetvalue(result, 0, 2)));
        data.equivalent = static_cast<SerializedEquivalent>(atoi(PQgetvalue(result, 0, 3)));
        data.isContractorGateway = atoi(PQgetvalue(result, 0, 4));
        
        PQclear(result);
        return data;
    }
    
    void insertTrustLineViaSQL(TrustLineID id, int state, ContractorID contractorID, 
                               SerializedEquivalent equivalent, int isContractorGateway) 
    {
        std::string query = "INSERT INTO " + tableName + 
                          " (id, state, contractor_id, equivalent, is_contractor_gateway) VALUES ($1, $2, $3, $4, $5);";
        
        std::string idStr = std::to_string(id);
        std::string stateStr = std::to_string(state);
        std::string contractorStr = std::to_string(contractorID);
        std::string equivalentStr = std::to_string(equivalent);
        std::string gatewayStr = std::to_string(isContractorGateway);
        
        const char *params[] = {idStr.c_str(), stateStr.c_str(), contractorStr.c_str(), equivalentStr.c_str(), gatewayStr.c_str()};
        int lengths[] = {0, 0, 0, 0, 0};
        int formats[] = {0, 0, 0, 0, 0};
        
        PGresult *result = PQexecParams(dbConnection, query.c_str(), 5, nullptr, params, lengths, formats, 0);
        
        if (PQresultStatus(result) != PGRES_COMMAND_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to insert trust line via SQL");
        }
        
        PQclear(result);
    }
    
    int countTrustLinesInDatabase() 
    {
        std::string query = "SELECT COUNT(*) FROM " + tableName + ";";
        PGresult *result = PQexec(dbConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            return -1;
        }
        
        int count = atoi(PQgetvalue(result, 0, 0));
        PQclear(result);
        
        return count;
    }

protected:
    std::unique_ptr<Logger> logger;
    std::unique_ptr<TrustLineHandlerPostgreSQL> handler;
    PGconn *dbConnection;
    std::string tableName;
    
    static int testCounter;
};

int TrustLineHandlerPostgreSQLTest::testCounter = 0;

// ============================================================================
// saveTrustLine Tests
// ============================================================================

TEST_F(TrustLineHandlerPostgreSQLTest, SaveTrustLine_ValidData_Success) 
{
    // Given
    auto trustLine = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID(),
        PostgreSQLTestFixtures::getValidContractorID()
    );
    auto equivalent = PostgreSQLTestFixtures::getValidEquivalent();
    
    // When
    EXPECT_NO_THROW(handler->saveTrustLine(trustLine, equivalent));
    
    // Then - Raw Database Data Validation
    EXPECT_TRUE(trustLineExistsInDatabase(trustLine->trustLineID(), trustLine->contractorID(), equivalent));
    
    auto rawData = getRawTrustLineData(trustLine->trustLineID(), trustLine->contractorID(), equivalent);
    EXPECT_EQ(rawData.id, trustLine->trustLineID());
    EXPECT_EQ(rawData.state, static_cast<int>(trustLine->state()));
    EXPECT_EQ(rawData.contractorID, trustLine->contractorID());
    EXPECT_EQ(rawData.equivalent, equivalent);
    EXPECT_EQ(rawData.isContractorGateway, trustLine->isContractorGateway() ? 1 : 0);
}

TEST_F(TrustLineHandlerPostgreSQLTest, SaveTrustLine_GatewayTrustLine_Success) 
{
    // Given
    auto trustLine = PostgreSQLTestFixtures::createGatewayTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID(),
        PostgreSQLTestFixtures::getValidContractorID()
    );
    auto equivalent = PostgreSQLTestFixtures::getValidEquivalent();
    
    // When
    EXPECT_NO_THROW(handler->saveTrustLine(trustLine, equivalent));
    
    // Then - Raw Database Data Validation
    auto rawData = getRawTrustLineData(trustLine->trustLineID(), trustLine->contractorID(), equivalent);
    EXPECT_EQ(rawData.isContractorGateway, 1);
    EXPECT_EQ(rawData.state, static_cast<int>(TrustLine::TrustLineState::Active));
}

TEST_F(TrustLineHandlerPostgreSQLTest, SaveTrustLine_DuplicateID_UpdatesExisting) 
{
    // Given
    auto trustLine1 = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID(),
        PostgreSQLTestFixtures::getValidContractorID()
    );
    auto trustLine2 = PostgreSQLTestFixtures::createTrustLineWithState(
        PostgreSQLTestFixtures::getValidTrustLineID(),
        PostgreSQLTestFixtures::getValidContractorID2(),
        TrustLine::TrustLineState::Active
    );
    auto equivalent = PostgreSQLTestFixtures::getValidEquivalent();
    
    // When
    EXPECT_NO_THROW(handler->saveTrustLine(trustLine1, equivalent));
    EXPECT_NO_THROW(handler->saveTrustLine(trustLine2, equivalent));
    
    // Then - Should update existing record
    EXPECT_EQ(countTrustLinesInDatabase(), 1);
    
    auto rawData = getRawTrustLineData(trustLine2->trustLineID(), trustLine2->contractorID(), equivalent);
    EXPECT_EQ(rawData.contractorID, trustLine2->contractorID());
    EXPECT_EQ(rawData.state, static_cast<int>(trustLine2->state()));
}

TEST_F(TrustLineHandlerPostgreSQLTest, SaveTrustLine_NullTrustLine_ThrowsException) 
{
    // Given
    TrustLine::Shared nullTrustLine = nullptr;
    auto equivalent = PostgreSQLTestFixtures::getValidEquivalent();
    
    // When & Then
    EXPECT_THROW(handler->saveTrustLine(nullTrustLine, equivalent), ValueError);
}

TEST_F(TrustLineHandlerPostgreSQLTest, SaveTrustLine_ReverseValidation_Success) 
{
    // Given - Insert via SQL
    TrustLineID id = PostgreSQLTestFixtures::getValidTrustLineID();
    ContractorID contractorID = PostgreSQLTestFixtures::getValidContractorID();
    SerializedEquivalent equivalent = PostgreSQLTestFixtures::getValidEquivalent();
    int state = static_cast<int>(TrustLine::TrustLineState::Active);
    int isGateway = 1;
    
    insertTrustLineViaSQL(id, state, contractorID, equivalent, isGateway);
    
    // When - Read via class method
    auto trustLines = handler->allTrustLinesByEquivalent(equivalent);
    
    // Then - Verify deserialization
    EXPECT_EQ(trustLines.size(), 1);
    EXPECT_EQ(trustLines[0]->trustLineID(), id);
    EXPECT_EQ(trustLines[0]->contractorID(), contractorID);
    EXPECT_EQ(trustLines[0]->state(), static_cast<TrustLine::TrustLineState>(state));
    EXPECT_EQ(trustLines[0]->isContractorGateway(), true);
}

// ============================================================================
// updateTrustLineState Tests
// ============================================================================

TEST_F(TrustLineHandlerPostgreSQLTest, UpdateTrustLineState_ValidData_Success) 
{
    // Given
    auto trustLine = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID(),
        PostgreSQLTestFixtures::getValidContractorID()
    );
    auto equivalent = PostgreSQLTestFixtures::getValidEquivalent();
    handler->saveTrustLine(trustLine, equivalent);
    
    // Update state
    trustLine->setState(TrustLine::TrustLineState::Active);
    
    // When
    EXPECT_NO_THROW(handler->updateTrustLineState(trustLine, equivalent));
    
    // Then - Raw Database Data Validation
    auto rawData = getRawTrustLineData(trustLine->trustLineID(), trustLine->contractorID(), equivalent);
    EXPECT_EQ(rawData.state, static_cast<int>(TrustLine::TrustLineState::Active));
}

TEST_F(TrustLineHandlerPostgreSQLTest, UpdateTrustLineState_NonexistentTrustLine_ThrowsException) 
{
    // Given
    auto trustLine = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID(),
        PostgreSQLTestFixtures::getValidContractorID()
    );
    auto equivalent = PostgreSQLTestFixtures::getValidEquivalent();
    
    // When & Then
    EXPECT_THROW(handler->updateTrustLineState(trustLine, equivalent), ValueError);
}

TEST_F(TrustLineHandlerPostgreSQLTest, UpdateTrustLineState_NullTrustLine_ThrowsException) 
{
    // Given
    TrustLine::Shared nullTrustLine = nullptr;
    auto equivalent = PostgreSQLTestFixtures::getValidEquivalent();
    
    // When & Then
    EXPECT_THROW(handler->updateTrustLineState(nullTrustLine, equivalent), ValueError);
}

// ============================================================================
// updateTrustLineIsContractorGateway Tests
// ============================================================================

TEST_F(TrustLineHandlerPostgreSQLTest, UpdateTrustLineIsContractorGateway_ValidData_Success) 
{
    // Given
    auto trustLine = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID(),
        PostgreSQLTestFixtures::getValidContractorID()
    );
    auto equivalent = PostgreSQLTestFixtures::getValidEquivalent();
    handler->saveTrustLine(trustLine, equivalent);
    
    // Update gateway status
    trustLine->setContractorAsGateway(true);
    
    // When
    EXPECT_NO_THROW(handler->updateTrustLineIsContractorGateway(trustLine, equivalent));
    
    // Then - Raw Database Data Validation
    auto rawData = getRawTrustLineData(trustLine->trustLineID(), trustLine->contractorID(), equivalent);
    EXPECT_EQ(rawData.isContractorGateway, 1);
}

TEST_F(TrustLineHandlerPostgreSQLTest, UpdateTrustLineIsContractorGateway_NonexistentTrustLine_ThrowsException) 
{
    // Given
    auto trustLine = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID(),
        PostgreSQLTestFixtures::getValidContractorID()
    );
    auto equivalent = PostgreSQLTestFixtures::getValidEquivalent();
    
    // When & Then
    EXPECT_THROW(handler->updateTrustLineIsContractorGateway(trustLine, equivalent), ValueError);
}

TEST_F(TrustLineHandlerPostgreSQLTest, UpdateTrustLineIsContractorGateway_NullTrustLine_ThrowsException) 
{
    // Given
    TrustLine::Shared nullTrustLine = nullptr;
    auto equivalent = PostgreSQLTestFixtures::getValidEquivalent();
    
    // When & Then
    EXPECT_THROW(handler->updateTrustLineIsContractorGateway(nullTrustLine, equivalent), ValueError);
}

// ============================================================================
// allTrustLinesByEquivalent Tests
// ============================================================================

TEST_F(TrustLineHandlerPostgreSQLTest, AllTrustLinesByEquivalent_ValidData_Success) 
{
    // Given
    auto equivalent = PostgreSQLTestFixtures::getValidEquivalent();
    auto trustLine1 = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID(),
        PostgreSQLTestFixtures::getValidContractorID()
    );
    auto trustLine2 = PostgreSQLTestFixtures::createGatewayTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID2(),
        PostgreSQLTestFixtures::getValidContractorID2()
    );
    
    handler->saveTrustLine(trustLine1, equivalent);
    handler->saveTrustLine(trustLine2, equivalent);
    
    // When
    auto result = handler->allTrustLinesByEquivalent(equivalent);
    
    // Then
    EXPECT_EQ(result.size(), 2);
    
    // Find trustLine1 in results
    auto it1 = std::find_if(result.begin(), result.end(), 
        [&](const TrustLine::Shared& tl) { return tl->trustLineID() == trustLine1->trustLineID(); });
    EXPECT_NE(it1, result.end());
    EXPECT_EQ((*it1)->contractorID(), trustLine1->contractorID());
    EXPECT_EQ((*it1)->state(), trustLine1->state());
    EXPECT_EQ((*it1)->isContractorGateway(), trustLine1->isContractorGateway());
    
    // Find trustLine2 in results
    auto it2 = std::find_if(result.begin(), result.end(), 
        [&](const TrustLine::Shared& tl) { return tl->trustLineID() == trustLine2->trustLineID(); });
    EXPECT_NE(it2, result.end());
    EXPECT_EQ((*it2)->contractorID(), trustLine2->contractorID());
    EXPECT_EQ((*it2)->state(), trustLine2->state());
    EXPECT_EQ((*it2)->isContractorGateway(), trustLine2->isContractorGateway());
}

TEST_F(TrustLineHandlerPostgreSQLTest, AllTrustLinesByEquivalent_NoData_ReturnsEmpty) 
{
    // Given
    auto equivalent = PostgreSQLTestFixtures::getValidEquivalent();
    
    // When
    auto result = handler->allTrustLinesByEquivalent(equivalent);
    
    // Then
    EXPECT_TRUE(result.empty());
}

TEST_F(TrustLineHandlerPostgreSQLTest, AllTrustLinesByEquivalent_DifferentEquivalents_ReturnsFiltered) 
{
    // Given
    auto equivalent1 = PostgreSQLTestFixtures::getValidEquivalent();
    auto equivalent2 = PostgreSQLTestFixtures::getValidEquivalent2();
    
    auto trustLine1 = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID(),
        PostgreSQLTestFixtures::getValidContractorID()
    );
    auto trustLine2 = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID2(),
        PostgreSQLTestFixtures::getValidContractorID2()
    );
    
    handler->saveTrustLine(trustLine1, equivalent1);
    handler->saveTrustLine(trustLine2, equivalent2);
    
    // When
    auto result1 = handler->allTrustLinesByEquivalent(equivalent1);
    auto result2 = handler->allTrustLinesByEquivalent(equivalent2);
    
    // Then
    EXPECT_EQ(result1.size(), 1);
    EXPECT_EQ(result2.size(), 1);
    EXPECT_EQ(result1[0]->trustLineID(), trustLine1->trustLineID());
    EXPECT_EQ(result2[0]->trustLineID(), trustLine2->trustLineID());
}

// ============================================================================
// deleteTrustLine Tests
// ============================================================================

TEST_F(TrustLineHandlerPostgreSQLTest, DeleteTrustLine_ValidData_Success) 
{
    // Given
    auto trustLine = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID(),
        PostgreSQLTestFixtures::getValidContractorID()
    );
    auto equivalent = PostgreSQLTestFixtures::getValidEquivalent();
    handler->saveTrustLine(trustLine, equivalent);
    
    // When
    EXPECT_NO_THROW(handler->deleteTrustLine(trustLine->contractorID(), equivalent));
    
    // Then - Raw Database Data Validation
    EXPECT_FALSE(trustLineExistsInDatabase(trustLine->trustLineID(), trustLine->contractorID(), equivalent));
    EXPECT_EQ(countTrustLinesInDatabase(), 0);
}

TEST_F(TrustLineHandlerPostgreSQLTest, DeleteTrustLine_NonexistentTrustLine_Success) 
{
    // Given
    auto contractorID = PostgreSQLTestFixtures::getValidContractorID();
    auto equivalent = PostgreSQLTestFixtures::getValidEquivalent();
    
    // When & Then - Should not throw
    EXPECT_NO_THROW(handler->deleteTrustLine(contractorID, equivalent));
}

TEST_F(TrustLineHandlerPostgreSQLTest, DeleteTrustLine_CrossMethodValidation_Success) 
{
    // Given
    auto trustLine = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID(),
        PostgreSQLTestFixtures::getValidContractorID()
    );
    auto equivalent = PostgreSQLTestFixtures::getValidEquivalent();
    handler->saveTrustLine(trustLine, equivalent);
    
    // When
    handler->deleteTrustLine(trustLine->contractorID(), equivalent);
    
    // Then - Cross-method validation
    auto result = handler->allTrustLinesByEquivalent(equivalent);
    EXPECT_TRUE(result.empty());
}

// ============================================================================
// equivalents Tests
// ============================================================================

TEST_F(TrustLineHandlerPostgreSQLTest, Equivalents_ValidData_Success) 
{
    // Given
    auto equivalent1 = PostgreSQLTestFixtures::getValidEquivalent();
    auto equivalent2 = PostgreSQLTestFixtures::getValidEquivalent2();
    auto equivalent3 = PostgreSQLTestFixtures::getValidEquivalent3();
    
    auto trustLine1 = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID(),
        PostgreSQLTestFixtures::getValidContractorID()
    );
    auto trustLine2 = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID2(),
        PostgreSQLTestFixtures::getValidContractorID2()
    );
    auto trustLine3 = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID3(),
        PostgreSQLTestFixtures::getValidContractorID3()
    );
    
    handler->saveTrustLine(trustLine1, equivalent1);
    handler->saveTrustLine(trustLine2, equivalent2);
    handler->saveTrustLine(trustLine3, equivalent3);
    
    // When
    auto result = handler->equivalents();
    
    // Then
    EXPECT_EQ(result.size(), 3);
    EXPECT_TRUE(std::find(result.begin(), result.end(), equivalent1) != result.end());
    EXPECT_TRUE(std::find(result.begin(), result.end(), equivalent2) != result.end());
    EXPECT_TRUE(std::find(result.begin(), result.end(), equivalent3) != result.end());
}

TEST_F(TrustLineHandlerPostgreSQLTest, Equivalents_NoData_ReturnsEmpty) 
{
    // When
    auto result = handler->equivalents();
    
    // Then
    EXPECT_TRUE(result.empty());
}

TEST_F(TrustLineHandlerPostgreSQLTest, Equivalents_DuplicateEquivalents_ReturnsUnique) 
{
    // Given
    auto equivalent = PostgreSQLTestFixtures::getValidEquivalent();
    
    auto trustLine1 = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID(),
        PostgreSQLTestFixtures::getValidContractorID()
    );
    auto trustLine2 = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID2(),
        PostgreSQLTestFixtures::getValidContractorID2()
    );
    
    handler->saveTrustLine(trustLine1, equivalent);
    handler->saveTrustLine(trustLine2, equivalent);
    
    // When
    auto result = handler->equivalents();
    
    // Then
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], equivalent);
}

// ============================================================================
// allIDs Tests
// ============================================================================

TEST_F(TrustLineHandlerPostgreSQLTest, AllIDs_ValidData_Success) 
{
    // Given
    auto trustLine1 = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID(),
        PostgreSQLTestFixtures::getValidContractorID()
    );
    auto trustLine2 = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID2(),
        PostgreSQLTestFixtures::getValidContractorID2()
    );
    
    handler->saveTrustLine(trustLine1, PostgreSQLTestFixtures::getValidEquivalent());
    handler->saveTrustLine(trustLine2, PostgreSQLTestFixtures::getValidEquivalent2());
    
    // When
    auto result = handler->allIDs();
    
    // Then
    EXPECT_EQ(result.size(), 2);
    EXPECT_TRUE(std::find(result.begin(), result.end(), trustLine1->trustLineID()) != result.end());
    EXPECT_TRUE(std::find(result.begin(), result.end(), trustLine2->trustLineID()) != result.end());
}

TEST_F(TrustLineHandlerPostgreSQLTest, AllIDs_NoData_ReturnsEmpty) 
{
    // When
    auto result = handler->allIDs();
    
    // Then
    EXPECT_TRUE(result.empty());
}

// ============================================================================
// allTrustLinesByContractor Tests
// ============================================================================

TEST_F(TrustLineHandlerPostgreSQLTest, AllTrustLinesByContractor_ValidData_Success) 
{
    // Given
    auto contractorID = PostgreSQLTestFixtures::getValidContractorID();
    auto trustLine1 = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID(),
        contractorID
    );
    auto trustLine2 = PostgreSQLTestFixtures::createGatewayTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID2(),
        contractorID
    );
    
    handler->saveTrustLine(trustLine1, PostgreSQLTestFixtures::getValidEquivalent());
    handler->saveTrustLine(trustLine2, PostgreSQLTestFixtures::getValidEquivalent2());
    
    // When
    auto result = handler->allTrustLinesByContractor(contractorID);
    
    // Then
    EXPECT_EQ(result.size(), 2);
    
    // Find trustLine1 in results
    auto it1 = std::find_if(result.begin(), result.end(), 
        [&](const TrustLine::Shared& tl) { return tl->trustLineID() == trustLine1->trustLineID(); });
    EXPECT_NE(it1, result.end());
    EXPECT_EQ((*it1)->contractorID(), contractorID);
    EXPECT_EQ((*it1)->state(), trustLine1->state());
    EXPECT_EQ((*it1)->isContractorGateway(), trustLine1->isContractorGateway());
    
    // Find trustLine2 in results
    auto it2 = std::find_if(result.begin(), result.end(), 
        [&](const TrustLine::Shared& tl) { return tl->trustLineID() == trustLine2->trustLineID(); });
    EXPECT_NE(it2, result.end());
    EXPECT_EQ((*it2)->contractorID(), contractorID);
    EXPECT_EQ((*it2)->state(), trustLine2->state());
    EXPECT_EQ((*it2)->isContractorGateway(), trustLine2->isContractorGateway());
}

TEST_F(TrustLineHandlerPostgreSQLTest, AllTrustLinesByContractor_NoData_ReturnsEmpty) 
{
    // Given
    auto contractorID = PostgreSQLTestFixtures::getValidContractorID();
    
    // When
    auto result = handler->allTrustLinesByContractor(contractorID);
    
    // Then
    EXPECT_TRUE(result.empty());
}

TEST_F(TrustLineHandlerPostgreSQLTest, AllTrustLinesByContractor_DifferentContractors_ReturnsFiltered) 
{
    // Given
    auto contractorID1 = PostgreSQLTestFixtures::getValidContractorID();
    auto contractorID2 = PostgreSQLTestFixtures::getValidContractorID2();
    
    auto trustLine1 = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID(),
        contractorID1
    );
    auto trustLine2 = PostgreSQLTestFixtures::createBasicTrustLine(
        PostgreSQLTestFixtures::getValidTrustLineID2(),
        contractorID2
    );
    
    handler->saveTrustLine(trustLine1, PostgreSQLTestFixtures::getValidEquivalent());
    handler->saveTrustLine(trustLine2, PostgreSQLTestFixtures::getValidEquivalent());
    
    // When
    auto result1 = handler->allTrustLinesByContractor(contractorID1);
    auto result2 = handler->allTrustLinesByContractor(contractorID2);
    
    // Then
    EXPECT_EQ(result1.size(), 1);
    EXPECT_EQ(result2.size(), 1);
    EXPECT_EQ(result1[0]->trustLineID(), trustLine1->trustLineID());
    EXPECT_EQ(result2[0]->trustLineID(), trustLine2->trustLineID());
} 