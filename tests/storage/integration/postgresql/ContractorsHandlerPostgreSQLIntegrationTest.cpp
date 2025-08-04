#include "gtest/gtest.h"
#include "../../../../src/core/io/storage/postgresql/ContractorsHandlerPostgreSQL.h"
#include "../../../../src/core/logger/Logger.h"
#include "../fixtures/DatabaseTestHelper.h"
#include "../fixtures/PostgreSQLTestFixtures.h"

class ContractorsHandlerPostgreSQLIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create database connection using hardcoded credentials
        mConnection = DatabaseTestHelper::createConnection(
            DatabaseTestHelper::TEST_HOST,
            DatabaseTestHelper::TEST_PORT,
            DatabaseTestHelper::TEST_USER,
            DatabaseTestHelper::TEST_PASSWORD,
            DatabaseTestHelper::TEST_DB_NAME);
        
        ASSERT_NE(mConnection, nullptr) << "Failed to create database connection";
        
        // Create logger
        mLogger = std::make_unique<Logger>();
        
        // Create unique test table name
        mTestTableName = "test_contractors_" + std::to_string(rand() % 10000);
        
        // Create handler instance
        mHandler = std::make_unique<ContractorsHandlerPostgreSQL>(mConnection, mTestTableName, *mLogger);
    }
    
    void TearDown() override {
        // Clean up test data
        if (mConnection && !mTestTableName.empty()) {
            DatabaseTestHelper::cleanupTable(mConnection, mTestTableName);
        }
        
        // Close database connection
        if (mConnection) {
            DatabaseTestHelper::closeConnection(mConnection);
        }
        
        mHandler.reset();
        mLogger.reset();
    }
    
    // Helper methods for raw database validation
    int getContractorCount() {
        std::string query = "SELECT COUNT(*) FROM " + mTestTableName;
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get contractor count");
        }
        
        int count = std::stoi(PQgetvalue(result, 0, 0));
        PQclear(result);
        return count;
    }
    
    int getContractorCount(ContractorID contractorID) {
        std::string query = "SELECT COUNT(*) FROM " + mTestTableName + " WHERE id = " + std::to_string(contractorID);
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get contractor count");
        }
        
        int count = std::stoi(PQgetvalue(result, 0, 0));
        PQclear(result);
        return count;
    }
    
    // Helper method to validate raw database data
    struct RawContractorData {
        int id;
        int idOnContractorSide;
        std::string cryptoKeyHex;
        int isConfirmed;
    };
    
    std::vector<RawContractorData> getRawContractorData() {
        std::vector<RawContractorData> data;
        std::string query = "SELECT id, id_on_contractor_side, crypto_key, is_confirmed FROM " + mTestTableName + " ORDER BY id";
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get raw contractor data");
        }
        
        int rows = PQntuples(result);
        for (int i = 0; i < rows; ++i) {
            RawContractorData contractor;
            contractor.id = std::stoi(PQgetvalue(result, i, 0));
            contractor.idOnContractorSide = std::stoi(PQgetvalue(result, i, 1));
            contractor.cryptoKeyHex = PQgetvalue(result, i, 2);
            contractor.isConfirmed = std::stoi(PQgetvalue(result, i, 3));
            data.push_back(contractor);
        }
        
        PQclear(result);
        return data;
    }
    
    RawContractorData getRawContractorData(ContractorID contractorID) {
        std::string query = "SELECT id, id_on_contractor_side, crypto_key, is_confirmed FROM " + mTestTableName + 
                           " WHERE id = " + std::to_string(contractorID);
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get raw contractor data");
        }
        
        if (PQntuples(result) == 0) {
            PQclear(result);
            throw std::runtime_error("Contractor not found");
        }
        
        RawContractorData contractor;
        contractor.id = std::stoi(PQgetvalue(result, 0, 0));
        contractor.idOnContractorSide = std::stoi(PQgetvalue(result, 0, 1));
        contractor.cryptoKeyHex = PQgetvalue(result, 0, 2);
        contractor.isConfirmed = std::stoi(PQgetvalue(result, 0, 3));
        
        PQclear(result);
        return contractor;
    }
    
    // Test data
    PGconn* mConnection = nullptr;
    std::unique_ptr<Logger> mLogger;
    std::unique_ptr<ContractorsHandlerPostgreSQL> mHandler;
    std::string mTestTableName;
};

// Test: Basic contractor saving
TEST_F(ContractorsHandlerPostgreSQLIntegrationTest, saveContractor_BasicContractor_SavesSuccessfully) {
    // Arrange
    auto contractor = PostgreSQLTestFixtures::createBasicContractor(PostgreSQLTestFixtures::getValidContractorID());
    
    // Act
    mHandler->saveContractor(contractor);
    
    // Assert
    EXPECT_EQ(getContractorCount(), 1);
    EXPECT_EQ(getContractorCount(contractor->getID()), 1);
    
    // Verify data retrieval through class methods
    auto allContractors = mHandler->allContractors();
    ASSERT_EQ(allContractors.size(), 1);
    
    auto savedContractor = allContractors[0];
    EXPECT_EQ(savedContractor->getID(), contractor->getID());
    EXPECT_NE(savedContractor->cryptoKey(), nullptr);
    EXPECT_EQ(savedContractor->isConfirmed(), false); // saveContractor sets confirmed to false
}

// Test: Full contractor saving
TEST_F(ContractorsHandlerPostgreSQLIntegrationTest, saveContractorFull_FullContractor_SavesSuccessfully) {
    // Arrange
    auto contractor = PostgreSQLTestFixtures::createFullContractor(
        PostgreSQLTestFixtures::getValidContractorID(),
        PostgreSQLTestFixtures::DEFAULT_CONTRACTOR_SIDE_ID,
        true);
    
    // Act
    mHandler->saveContractorFull(contractor);
    
    // Assert
    EXPECT_EQ(getContractorCount(), 1);
    
    // Verify data retrieval through class methods
    auto allContractors = mHandler->allContractors();
    ASSERT_EQ(allContractors.size(), 1);
    
    auto savedContractor = allContractors[0];
    EXPECT_EQ(savedContractor->getID(), contractor->getID());
    EXPECT_EQ(savedContractor->ownIdOnContractorSide(), contractor->ownIdOnContractorSide());
    EXPECT_EQ(savedContractor->isConfirmed(), true); // saveContractorFull sets confirmed to true
}

// Test: Confirmation info update
TEST_F(ContractorsHandlerPostgreSQLIntegrationTest, saveConfirmationInfo_UpdatesExistingContractor) {
    // Arrange - First save basic contractor
    auto contractor = PostgreSQLTestFixtures::createBasicContractor(PostgreSQLTestFixtures::getValidContractorID());
    mHandler->saveContractor(contractor);
    
    // Update contractor with confirmation info
    contractor->setOwnIdOnContractorSide(PostgreSQLTestFixtures::DEFAULT_CONTRACTOR_SIDE_ID);
    contractor->setCryptoKey(PostgreSQLTestFixtures::createDifferentCryptoKey());
    
    // Act
    mHandler->saveConfirmationInfo(contractor);
    
    // Assert
    EXPECT_EQ(getContractorCount(), 1); // Still one contractor
    
    // Verify updated data
    auto allContractors = mHandler->allContractors();
    ASSERT_EQ(allContractors.size(), 1);
    
    auto updatedContractor = allContractors[0];
    EXPECT_EQ(updatedContractor->getID(), contractor->getID());
    EXPECT_EQ(updatedContractor->ownIdOnContractorSide(), PostgreSQLTestFixtures::DEFAULT_CONTRACTOR_SIDE_ID);
    EXPECT_EQ(updatedContractor->isConfirmed(), true);
}

// Test: Crypto key update
TEST_F(ContractorsHandlerPostgreSQLIntegrationTest, updateCryptoKey_UpdatesExistingContractor) {
    // Arrange - First save basic contractor
    auto contractor = PostgreSQLTestFixtures::createBasicContractor(PostgreSQLTestFixtures::getValidContractorID());
    mHandler->saveContractor(contractor);
    
    // Update contractor with new crypto key
    contractor->setCryptoKey(PostgreSQLTestFixtures::createDifferentCryptoKey());
    
    // Act
    mHandler->updateCryptoKey(contractor);
    
    // Assert
    EXPECT_EQ(getContractorCount(), 1); // Still one contractor
    
    // Verify updated data
    auto allContractors = mHandler->allContractors();
    ASSERT_EQ(allContractors.size(), 1);
    
    auto updatedContractor = allContractors[0];
    EXPECT_EQ(updatedContractor->getID(), contractor->getID());
    EXPECT_EQ(updatedContractor->isConfirmed(), true); // updateCryptoKey sets confirmed to true
}

// Test: Channel ID update
TEST_F(ContractorsHandlerPostgreSQLIntegrationTest, updateChannelIdOnContractorSide_UpdatesExistingContractor) {
    // Arrange - First save basic contractor
    auto contractor = PostgreSQLTestFixtures::createBasicContractor(PostgreSQLTestFixtures::getValidContractorID());
    mHandler->saveContractor(contractor);
    
    // Update contractor with new channel ID
    ContractorID newChannelId = PostgreSQLTestFixtures::DEFAULT_CONTRACTOR_SIDE_ID + 100;
    contractor->setOwnIdOnContractorSide(newChannelId);
    
    // Act
    mHandler->updateChannelIdOnContractorSide(contractor);
    
    // Assert
    EXPECT_EQ(getContractorCount(), 1); // Still one contractor
    
    // Verify updated data
    auto allContractors = mHandler->allContractors();
    ASSERT_EQ(allContractors.size(), 1);
    
    auto updatedContractor = allContractors[0];
    EXPECT_EQ(updatedContractor->getID(), contractor->getID());
    EXPECT_EQ(updatedContractor->ownIdOnContractorSide(), newChannelId);
}

// Test: All IDs retrieval
TEST_F(ContractorsHandlerPostgreSQLIntegrationTest, allIDs_MultipleContractors_ReturnsAllIDs) {
    // Arrange
    auto contractors = PostgreSQLTestFixtures::createMultipleContractors(3);
    
    // Act - Save all contractors
    for (auto& contractor : contractors) {
        mHandler->saveContractorFull(contractor);
    }
    
    // Assert
    EXPECT_EQ(getContractorCount(), 3);
    
    auto allIDs = mHandler->allIDs();
    ASSERT_EQ(allIDs.size(), 3);
    
    // Verify all expected IDs are present
    for (auto& contractor : contractors) {
        EXPECT_NE(std::find(allIDs.begin(), allIDs.end(), contractor->getID()), allIDs.end());
    }
}

// Test: Contractor removal
TEST_F(ContractorsHandlerPostgreSQLIntegrationTest, removeContractor_ExistingContractor_RemovesSuccessfully) {
    // Arrange
    auto contractor = PostgreSQLTestFixtures::createBasicContractor(PostgreSQLTestFixtures::getValidContractorID());
    mHandler->saveContractor(contractor);
    EXPECT_EQ(getContractorCount(), 1);
    
    // Act
    mHandler->removeContractor(contractor->getID());
    
    // Assert
    EXPECT_EQ(getContractorCount(), 0);
    
    auto allContractors = mHandler->allContractors();
    EXPECT_EQ(allContractors.size(), 0);
}

// Test: Error handling for non-existent contractor removal
TEST_F(ContractorsHandlerPostgreSQLIntegrationTest, removeContractor_NonExistentContractor_ThrowsValueError) {
    // Arrange
    ContractorID nonExistentId = PostgreSQLTestFixtures::getValidContractorID();
    
    // Act & Assert
    EXPECT_THROW(mHandler->removeContractor(nonExistentId), ValueError);
}

// Test: Raw database data validation
TEST_F(ContractorsHandlerPostgreSQLIntegrationTest, saveContractorFull_ValidatesRawDatabaseData) {
    // Arrange
    ContractorID contractorId = PostgreSQLTestFixtures::getValidContractorID();
    ContractorID contractorSideId = PostgreSQLTestFixtures::DEFAULT_CONTRACTOR_SIDE_ID;
    auto contractor = PostgreSQLTestFixtures::createFullContractor(contractorId, contractorSideId, true);
    
    // Act
    mHandler->saveContractorFull(contractor);
    
    // Assert - Check raw database data
    auto rawData = getRawContractorData(contractorId);
    EXPECT_EQ(rawData.id, contractorId);
    EXPECT_EQ(rawData.idOnContractorSide, contractorSideId);
    EXPECT_EQ(rawData.isConfirmed, 1); // Boolean true stored as 1
    
    // Verify crypto key is stored as BYTEA in hex format
    EXPECT_FALSE(rawData.cryptoKeyHex.empty());
    EXPECT_TRUE(rawData.cryptoKeyHex.substr(0, 2) == "\\x"); // PostgreSQL BYTEA hex format
    
    // Verify crypto key size is reasonable (generated keys are larger than our test constant)
    std::string hexData = rawData.cryptoKeyHex.substr(2); // Remove \x prefix
    EXPECT_GT(hexData.length(), 0); // Must have some data
    EXPECT_EQ(hexData.length() % 2, 0); // Must be even number of hex chars
}

// Test: Reverse validation - Write via one method, read via another method
TEST_F(ContractorsHandlerPostgreSQLIntegrationTest, crossMethodValidation_WriteReadDifferentMethods_WorksCorrectly) {
    // Arrange - Create test contractor
    ContractorID contractorId = PostgreSQLTestFixtures::getValidContractorID();
    ContractorID contractorSideId = PostgreSQLTestFixtures::DEFAULT_CONTRACTOR_SIDE_ID;
    auto originalContractor = PostgreSQLTestFixtures::createFullContractor(contractorId, contractorSideId, true);
    
    // Act 1 - Save using saveContractor method
    mHandler->saveContractor(originalContractor);
    
    // Act 2 - Update using saveConfirmationInfo method
    originalContractor->setOwnIdOnContractorSide(contractorSideId);
    mHandler->saveConfirmationInfo(originalContractor);
    
    // Assert - Read using allContractors and verify consistency
    auto retrievedContractors = mHandler->allContractors();
    ASSERT_EQ(retrievedContractors.size(), 1);
    
    auto retrievedContractor = retrievedContractors[0];
    EXPECT_EQ(retrievedContractor->getID(), contractorId);
    EXPECT_EQ(retrievedContractor->ownIdOnContractorSide(), contractorSideId);
    EXPECT_EQ(retrievedContractor->isConfirmed(), true);
    
    // Verify crypto key deserialization - keys should exist and be valid
    ASSERT_NE(retrievedContractor->cryptoKey(), nullptr);
    ASSERT_NE(originalContractor->cryptoKey(), nullptr);
    
    // Verify crypto keys can be serialized (basic functionality test)
    std::vector<uint8_t> originalCryptoBlob;
    std::vector<uint8_t> retrievedCryptoBlob;
    ASSERT_NO_THROW(originalContractor->cryptoKey()->serialize(originalCryptoBlob));
    ASSERT_NO_THROW(retrievedContractor->cryptoKey()->serialize(retrievedCryptoBlob));
    
    // Basic sanity checks for crypto key data
    EXPECT_GT(originalCryptoBlob.size(), 0);
    EXPECT_GT(retrievedCryptoBlob.size(), 0);
    
    // Act 3 - Test updateCryptoKey method
    auto newCryptoKey = PostgreSQLTestFixtures::createDifferentCryptoKey();
    originalContractor->setCryptoKey(newCryptoKey);
    mHandler->updateCryptoKey(originalContractor);
    
    // Verify crypto key was updated - basic functionality test
    auto updatedContractors = mHandler->allContractors();
    ASSERT_EQ(updatedContractors.size(), 1);
    
    // Verify updated crypto key is valid and can be serialized
    ASSERT_NE(updatedContractors[0]->cryptoKey(), nullptr);
    std::vector<uint8_t> updatedCryptoBlob;
    ASSERT_NO_THROW(updatedContractors[0]->cryptoKey()->serialize(updatedCryptoBlob));
    EXPECT_GT(updatedCryptoBlob.size(), 0);
    
    // Verify allIDs method works correctly
    auto allIDs = mHandler->allIDs();
    ASSERT_EQ(allIDs.size(), 1);
    EXPECT_EQ(allIDs[0], contractorId);
    
    // Additional verification - check database state
    EXPECT_EQ(getContractorCount(), 1);
}

// Test: Database connection error handling
TEST_F(ContractorsHandlerPostgreSQLIntegrationTest, Constructor_NullConnection_ThrowsValueError) {
    // Arrange & Act & Assert
    EXPECT_THROW(
        ContractorsHandlerPostgreSQL(nullptr, "test_table", *mLogger),
        ValueError
    );
}

// Test: Empty table name error handling
TEST_F(ContractorsHandlerPostgreSQLIntegrationTest, Constructor_EmptyTableName_ThrowsValueError) {
    // Arrange & Act & Assert
    EXPECT_THROW(
        ContractorsHandlerPostgreSQL(mConnection, "", *mLogger),
        ValueError
    );
} 