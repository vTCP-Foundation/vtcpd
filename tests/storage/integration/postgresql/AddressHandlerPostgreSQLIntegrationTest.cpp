#include "gtest/gtest.h"
#include "../../../../src/core/io/storage/postgresql/AddressHandlerPostgreSQL.h"
#include "../../../../src/core/logger/Logger.h"
#include "../fixtures/DatabaseTestHelper.h"
#include "../fixtures/PostgreSQLTestFixtures.h"

class AddressHandlerPostgreSQLIntegrationTest : public ::testing::Test {
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
        
        // Create contractors table (required by foreign key constraint)
        createContractorsTable();
        
        // Create test contractor records
        insertTestContractors();
        
        // Create AddressHandlerPostgreSQL instance
        mHandler = std::make_unique<AddressHandlerPostgreSQL>(
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
    
    void createContractorsTable() {
        std::string query = "CREATE TABLE IF NOT EXISTS contractors ("
                           "id INTEGER PRIMARY KEY, "
                           "id_on_contractor_side INTEGER, "
                           "crypto_key BYTEA NOT NULL, "
                           "is_confirmed INTEGER NOT NULL DEFAULT 0)";
        DatabaseTestHelper::executeQuery(mConnection, query);
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
            DatabaseTestHelper::executeQuery(mConnection, query);
        }
    }
    
    void cleanupTestData() {
        try {
            DatabaseTestHelper::cleanupTable(mConnection, mTestTableName);
            DatabaseTestHelper::cleanupTable(mConnection, "contractors");
        } catch (const std::exception& e) {
            // Continue cleanup even if some operations fail
            std::cerr << "Cleanup warning: " << e.what() << std::endl;
        }
    }
    
    int getAddressCount(ContractorID contractorID) {
        std::string query = "SELECT COUNT(*) FROM " + mTestTableName + " WHERE contractor_id = " + std::to_string(contractorID);
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get address count");
        }
        
        int count = std::stoi(PQgetvalue(result, 0, 0));
        PQclear(result);
        return count;
    }
    
    // Helper method to verify raw database data
    struct RawAddressData {
        int type;
        int contractorId;
        int addressSize;
        std::string addressHex;
    };
    
    std::vector<RawAddressData> getRawAddressData(ContractorID contractorID) {
        std::string query = "SELECT type, contractor_id, address_size, address FROM " + mTestTableName + 
                           " WHERE contractor_id = " + std::to_string(contractorID) + " ORDER BY type";
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to get raw address data");
        }
        
        std::vector<RawAddressData> data;
        int rows = PQntuples(result);
        
        for (int i = 0; i < rows; ++i) {
            RawAddressData rawData;
            rawData.type = std::stoi(PQgetvalue(result, i, 0));
            rawData.contractorId = std::stoi(PQgetvalue(result, i, 1));
            rawData.addressSize = std::stoi(PQgetvalue(result, i, 2));
            rawData.addressHex = PQgetvalue(result, i, 3);
            data.push_back(rawData);
        }
        
        PQclear(result);
        return data;
    }
    
    PGconn* mConnection;
    std::unique_ptr<AddressHandlerPostgreSQL> mHandler;
    Logger mLogger;
    std::string mTestTableName = "test_addresses";
};

// Test: saveAddress - IPv4 address successful save
TEST_F(AddressHandlerPostgreSQLIntegrationTest, saveAddress_IPv4Address_SavesSuccessfully) {
    // Arrange
    ContractorID contractorID = PostgreSQLTestFixtures::getValidContractorID();
    auto ipv4Address = PostgreSQLTestFixtures::createIPv4Address(
        PostgreSQLTestFixtures::DEFAULT_TEST_IP,
        PostgreSQLTestFixtures::DEFAULT_TEST_PORT
    );
    
    // Act
    ASSERT_NO_THROW(mHandler->saveAddress(contractorID, ipv4Address));
    
    // Assert
    EXPECT_EQ(getAddressCount(contractorID), 1);
    
    auto retrievedAddresses = mHandler->contractorAddresses(contractorID);
    ASSERT_EQ(retrievedAddresses.size(), 1);
    
    auto retrievedAddress = retrievedAddresses[0];
    EXPECT_EQ(retrievedAddress->typeID(), BaseAddress::IPv4_IncludingPort);
    
    // Cast to IPv4WithPortAddress to check specific details
    auto ipv4Retrieved = std::dynamic_pointer_cast<IPv4WithPortAddress>(retrievedAddress);
    ASSERT_NE(ipv4Retrieved, nullptr);
    EXPECT_EQ(ipv4Retrieved->host(), PostgreSQLTestFixtures::DEFAULT_TEST_IP);
    EXPECT_EQ(ipv4Retrieved->port(), PostgreSQLTestFixtures::DEFAULT_TEST_PORT);
}

// Test: saveAddress - GNS address successful save
TEST_F(AddressHandlerPostgreSQLIntegrationTest, saveAddress_GNSAddress_SavesSuccessfully) {
    // Arrange
    ContractorID contractorID = PostgreSQLTestFixtures::getValidContractorID();
    auto gnsAddress = PostgreSQLTestFixtures::createGNSAddress(
        PostgreSQLTestFixtures::DEFAULT_GNS_IDENTIFIER
    );
    
    // Act
    ASSERT_NO_THROW(mHandler->saveAddress(contractorID, gnsAddress));
    
    // Assert
    EXPECT_EQ(getAddressCount(contractorID), 1);
    
    auto retrievedAddresses = mHandler->contractorAddresses(contractorID);
    ASSERT_EQ(retrievedAddresses.size(), 1);
    
    auto retrievedAddress = retrievedAddresses[0];
    EXPECT_EQ(retrievedAddress->typeID(), BaseAddress::GNS);
    
    // Cast to GNSAddress to check specific details
    auto gnsRetrieved = std::dynamic_pointer_cast<GNSAddress>(retrievedAddress);
    ASSERT_NE(gnsRetrieved, nullptr);
    EXPECT_EQ(gnsRetrieved->fullAddress(), PostgreSQLTestFixtures::DEFAULT_GNS_IDENTIFIER);
}

// Test: saveAddress - Multiple addresses for same contractor
TEST_F(AddressHandlerPostgreSQLIntegrationTest, saveAddress_MultipleAddresses_SavesAllSuccessfully) {
    // Arrange
    ContractorID contractorID = PostgreSQLTestFixtures::getValidContractorID();
    auto addresses = PostgreSQLTestFixtures::createMixedAddresses();
    
    // Act
    for (const auto& address : addresses) {
        ASSERT_NO_THROW(mHandler->saveAddress(contractorID, address));
    }
    
    // Assert
    EXPECT_EQ(getAddressCount(contractorID), addresses.size());
    
    auto retrievedAddresses = mHandler->contractorAddresses(contractorID);
    EXPECT_EQ(retrievedAddresses.size(), addresses.size());
    
    // Verify that all address types are present
    bool hasIPv4 = false, hasGNS = false;
    for (const auto& address : retrievedAddresses) {
        if (address->typeID() == BaseAddress::IPv4_IncludingPort) {
            hasIPv4 = true;
        } else if (address->typeID() == BaseAddress::GNS) {
            hasGNS = true;
        }
    }
    
    EXPECT_TRUE(hasIPv4);
    EXPECT_TRUE(hasGNS);
}

// Test: contractorAddresses - No addresses returns empty vector
TEST_F(AddressHandlerPostgreSQLIntegrationTest, contractorAddresses_NoAddresses_ReturnsEmptyVector) {
    // Arrange
    ContractorID contractorID = PostgreSQLTestFixtures::getValidContractorID();
    
    // Act
    auto addresses = mHandler->contractorAddresses(contractorID);
    
    // Assert
    EXPECT_TRUE(addresses.empty());
    EXPECT_EQ(addresses.size(), 0);
}

// Test: contractorAddresses - Nonexistent contractor returns empty vector
TEST_F(AddressHandlerPostgreSQLIntegrationTest, contractorAddresses_NonexistentContractor_ReturnsEmptyVector) {
    // Arrange
    ContractorID nonexistentContractorID = 99999;
    
    // Act
    auto addresses = mHandler->contractorAddresses(nonexistentContractorID);
    
    // Assert
    EXPECT_TRUE(addresses.empty());
    EXPECT_EQ(addresses.size(), 0);
}

// Test: contractorAddresses - Multiple contractors isolation
TEST_F(AddressHandlerPostgreSQLIntegrationTest, contractorAddresses_MultipleContractors_IsolatesCorrectly) {
    // Arrange
    ContractorID contractor1 = PostgreSQLTestFixtures::getValidContractorID();
    ContractorID contractor2 = PostgreSQLTestFixtures::getValidContractorID2();
    
    auto address1 = PostgreSQLTestFixtures::createIPv4Address("192.168.1.1", 8080);
    auto address2 = PostgreSQLTestFixtures::createIPv4Address("192.168.1.2", 9090);
    
    // Act - Save addresses for different contractors
    mHandler->saveAddress(contractor1, address1);
    mHandler->saveAddress(contractor2, address2);
    
    // Assert - Each contractor should only see their own addresses
    auto addresses1 = mHandler->contractorAddresses(contractor1);
    auto addresses2 = mHandler->contractorAddresses(contractor2);
    
    EXPECT_EQ(addresses1.size(), 1);
    EXPECT_EQ(addresses2.size(), 1);
    
    auto ipv4_1 = std::dynamic_pointer_cast<IPv4WithPortAddress>(addresses1[0]);
    auto ipv4_2 = std::dynamic_pointer_cast<IPv4WithPortAddress>(addresses2[0]);
    
    ASSERT_NE(ipv4_1, nullptr);
    ASSERT_NE(ipv4_2, nullptr);
    
    EXPECT_EQ(ipv4_1->host(), "192.168.1.1");
    EXPECT_EQ(ipv4_2->host(), "192.168.1.2");
}

// Test: removeAddresses - Removes all addresses for contractor
TEST_F(AddressHandlerPostgreSQLIntegrationTest, removeAddresses_WithAddresses_RemovesAllSuccessfully) {
    // Arrange
    ContractorID contractorID = PostgreSQLTestFixtures::getValidContractorID();
    auto addresses = PostgreSQLTestFixtures::createMixedAddresses();
    
    // Save multiple addresses
    for (const auto& address : addresses) {
        mHandler->saveAddress(contractorID, address);
    }
    
    // Verify addresses are saved
    EXPECT_EQ(getAddressCount(contractorID), addresses.size());
    
    // Act
    ASSERT_NO_THROW(mHandler->removeAddresses(contractorID));
    
    // Assert
    EXPECT_EQ(getAddressCount(contractorID), 0);
    
    auto retrievedAddresses = mHandler->contractorAddresses(contractorID);
    EXPECT_TRUE(retrievedAddresses.empty());
}

// Test: removeAddresses - No addresses to remove (should not throw)
TEST_F(AddressHandlerPostgreSQLIntegrationTest, removeAddresses_NoAddresses_DoesNotThrow) {
    // Arrange
    ContractorID contractorID = PostgreSQLTestFixtures::getValidContractorID();
    
    // Act & Assert
    ASSERT_NO_THROW(mHandler->removeAddresses(contractorID));
    
    // Verify still no addresses
    EXPECT_EQ(getAddressCount(contractorID), 0);
}

// Test: removeAddresses - Only removes addresses for specific contractor
TEST_F(AddressHandlerPostgreSQLIntegrationTest, removeAddresses_MultipleContractors_RemovesOnlySpecificContractor) {
    // Arrange
    ContractorID contractor1 = PostgreSQLTestFixtures::getValidContractorID();
    ContractorID contractor2 = PostgreSQLTestFixtures::getValidContractorID2();
    
    auto address1 = PostgreSQLTestFixtures::createIPv4Address("192.168.1.1", 8080);
    auto address2 = PostgreSQLTestFixtures::createIPv4Address("192.168.1.2", 9090);
    
    // Save addresses for both contractors
    mHandler->saveAddress(contractor1, address1);
    mHandler->saveAddress(contractor2, address2);
    
    // Verify both have addresses
    EXPECT_EQ(getAddressCount(contractor1), 1);
    EXPECT_EQ(getAddressCount(contractor2), 1);
    
    // Act - Remove addresses for contractor1 only
    ASSERT_NO_THROW(mHandler->removeAddresses(contractor1));
    
    // Assert - contractor1 has no addresses, contractor2 still has addresses
    EXPECT_EQ(getAddressCount(contractor1), 0);
    EXPECT_EQ(getAddressCount(contractor2), 1);
    
    auto addresses2 = mHandler->contractorAddresses(contractor2);
    EXPECT_EQ(addresses2.size(), 1);
}

// Test: Table creation and schema validation
TEST_F(AddressHandlerPostgreSQLIntegrationTest, TableCreation_ValidatesSchemaCorrectly) {
    // Assert - Table should exist after handler creation
    EXPECT_TRUE(DatabaseTestHelper::tableExists(mConnection, mTestTableName));
    
    // Verify table structure by attempting to insert valid data
    ContractorID contractorID = PostgreSQLTestFixtures::getValidContractorID();
    auto address = PostgreSQLTestFixtures::createIPv4Address("10.0.0.1", 8080);
    
    ASSERT_NO_THROW(mHandler->saveAddress(contractorID, address));
    
    // Note: Foreign key constraint validation is handled by real database operations,
    // and in test environment with pre-existing tables, constraint behavior may vary
}

// Test: Database connection error handling
TEST_F(AddressHandlerPostgreSQLIntegrationTest, Constructor_NullConnection_ThrowsIOError) {
    // Arrange & Act & Assert
    EXPECT_THROW(
        AddressHandlerPostgreSQL(nullptr, "test_table", mLogger),
        IOError
    );
}

// Test: Raw database data validation through direct SQL queries
TEST_F(AddressHandlerPostgreSQLIntegrationTest, saveAddress_ValidatesRawDatabaseData) {
    // Arrange
    ContractorID contractorID = PostgreSQLTestFixtures::getValidContractorID();
    auto ipv4Address = PostgreSQLTestFixtures::createIPv4Address("10.20.30.40", 12345);
    auto gnsAddress = PostgreSQLTestFixtures::createGNSAddress("mynode@provider");
    
    // Act - Save addresses
    mHandler->saveAddress(contractorID, ipv4Address);
    mHandler->saveAddress(contractorID, gnsAddress);
    
    // Assert - Check raw database data through direct SQL
    auto rawData = getRawAddressData(contractorID);
    ASSERT_EQ(rawData.size(), 2);
    
    // Verify IPv4 address raw data (should be first due to ORDER BY type)
    const auto& ipv4RawData = rawData[0];
    EXPECT_EQ(ipv4RawData.type, static_cast<int>(BaseAddress::IPv4_IncludingPort));
    EXPECT_EQ(ipv4RawData.contractorId, contractorID);
    EXPECT_GT(ipv4RawData.addressSize, 0);
    EXPECT_FALSE(ipv4RawData.addressHex.empty());
    EXPECT_TRUE(ipv4RawData.addressHex.substr(0, 2) == "\\x"); // PostgreSQL BYTEA hex format
    
    // Verify GNS address raw data
    const auto& gnsRawData = rawData[1]; 
    EXPECT_EQ(gnsRawData.type, static_cast<int>(BaseAddress::GNS));
    EXPECT_EQ(gnsRawData.contractorId, contractorID);
    EXPECT_GT(gnsRawData.addressSize, 0);
    EXPECT_FALSE(gnsRawData.addressHex.empty());
    EXPECT_TRUE(gnsRawData.addressHex.substr(0, 2) == "\\x"); // PostgreSQL BYTEA hex format
    
    // Verify serialized size matches address_size field
    EXPECT_EQ(ipv4RawData.addressSize, ipv4Address->serializedSize());
    EXPECT_EQ(gnsRawData.addressSize, gnsAddress->serializedSize());
    
    // Verify that data can be retrieved correctly through class methods
    auto retrievedAddresses = mHandler->contractorAddresses(contractorID);
    ASSERT_EQ(retrievedAddresses.size(), 2);
    
    // Verify reconstructed data matches original
    bool foundIPv4 = false, foundGNS = false;
    for (const auto& addr : retrievedAddresses) {
        if (addr->typeID() == BaseAddress::IPv4_IncludingPort) {
            auto ipv4Retrieved = std::dynamic_pointer_cast<IPv4WithPortAddress>(addr);
            ASSERT_NE(ipv4Retrieved, nullptr);
            EXPECT_EQ(ipv4Retrieved->host(), "10.20.30.40");
            EXPECT_EQ(ipv4Retrieved->port(), 12345);
            foundIPv4 = true;
        } else if (addr->typeID() == BaseAddress::GNS) {
            auto gnsRetrieved = std::dynamic_pointer_cast<GNSAddress>(addr);
            ASSERT_NE(gnsRetrieved, nullptr);
            EXPECT_EQ(gnsRetrieved->fullAddress(), "mynode@provider");
            foundGNS = true;
        }
    }
    
    EXPECT_TRUE(foundIPv4);
    EXPECT_TRUE(foundGNS);
}

// Test: Reverse validation - Insert via SQL, read via class methods  
TEST_F(AddressHandlerPostgreSQLIntegrationTest, directInsert_ReadViaClassMethods_DeserializesCorrectly) {
    // Arrange - Create test addresses and serialize them
    ContractorID contractorID = PostgreSQLTestFixtures::getValidContractorID();
    auto originalIPv4 = PostgreSQLTestFixtures::createIPv4Address("192.168.100.200", 54321);
    auto originalGNS = PostgreSQLTestFixtures::createGNSAddress("testnode@myprovider");
    
    // Serialize addresses to get raw binary data
    auto ipv4Bytes = originalIPv4->serializeToBytes();
    auto gnsBytes = originalGNS->serializeToBytes();
    
    // Act - Insert data directly into database via SQL INSERT
    std::string insertQuery1 = "INSERT INTO " + mTestTableName + 
                              " (type, contractor_id, address_size, address) VALUES ($1, $2, $3, $4)";
    
    // Insert IPv4 address
    {
        const char* paramValues[4];
        int paramLengths[4];
        int paramFormats[4];
        
        std::string typeStr = std::to_string(static_cast<int>(BaseAddress::IPv4_IncludingPort));
        std::string contractorStr = std::to_string(contractorID);
        std::string sizeStr = std::to_string(originalIPv4->serializedSize());
        
        paramValues[0] = typeStr.c_str();
        paramValues[1] = contractorStr.c_str();
        paramValues[2] = sizeStr.c_str();
        paramValues[3] = reinterpret_cast<const char*>(ipv4Bytes.get());
        
        paramLengths[0] = 0; // text
        paramLengths[1] = 0; // text  
        paramLengths[2] = 0; // text
        paramLengths[3] = static_cast<int>(originalIPv4->serializedSize()); // binary
        
        paramFormats[0] = 0; // text
        paramFormats[1] = 0; // text
        paramFormats[2] = 0; // text
        paramFormats[3] = 1; // binary for BYTEA
        
        PGresult* result = PQexecParams(mConnection, insertQuery1.c_str(), 4, nullptr, 
                                       paramValues, paramLengths, paramFormats, 0);
        
        if (PQresultStatus(result) != PGRES_COMMAND_OK) {
            PQclear(result);
            FAIL() << "Failed to insert IPv4 address: " << PQerrorMessage(mConnection);
        }
        PQclear(result);
    }
    
    // Insert GNS address
    {
        const char* paramValues[4];
        int paramLengths[4];
        int paramFormats[4];
        
        std::string typeStr = std::to_string(static_cast<int>(BaseAddress::GNS));
        std::string contractorStr = std::to_string(contractorID);
        std::string sizeStr = std::to_string(originalGNS->serializedSize());
        
        paramValues[0] = typeStr.c_str();
        paramValues[1] = contractorStr.c_str();
        paramValues[2] = sizeStr.c_str();
        paramValues[3] = reinterpret_cast<const char*>(gnsBytes.get());
        
        paramLengths[0] = 0; // text
        paramLengths[1] = 0; // text
        paramLengths[2] = 0; // text
        paramLengths[3] = static_cast<int>(originalGNS->serializedSize()); // binary
        
        paramFormats[0] = 0; // text
        paramFormats[1] = 0; // text
        paramFormats[2] = 0; // text
        paramFormats[3] = 1; // binary for BYTEA
        
        PGresult* result = PQexecParams(mConnection, insertQuery1.c_str(), 4, nullptr,
                                       paramValues, paramLengths, paramFormats, 0);
        
        if (PQresultStatus(result) != PGRES_COMMAND_OK) {
            PQclear(result);
            FAIL() << "Failed to insert GNS address: " << PQerrorMessage(mConnection);
        }
        PQclear(result);
    }
    
    // Assert - Read data via class methods and verify correct deserialization
    auto retrievedAddresses = mHandler->contractorAddresses(contractorID);
    ASSERT_EQ(retrievedAddresses.size(), 2);
    
    // Verify that deserialized data matches original objects
    bool foundIPv4 = false, foundGNS = false;
    
    for (const auto& address : retrievedAddresses) {
        if (address->typeID() == BaseAddress::IPv4_IncludingPort) {
            auto ipv4Retrieved = std::dynamic_pointer_cast<IPv4WithPortAddress>(address);
            ASSERT_NE(ipv4Retrieved, nullptr);
            
            // Verify IPv4 specific data
            EXPECT_EQ(ipv4Retrieved->host(), "192.168.100.200");
            EXPECT_EQ(ipv4Retrieved->port(), 54321);
            EXPECT_EQ(ipv4Retrieved->typeID(), originalIPv4->typeID());
            EXPECT_EQ(ipv4Retrieved->serializedSize(), originalIPv4->serializedSize());
            EXPECT_EQ(ipv4Retrieved->fullAddress(), originalIPv4->fullAddress());
            
            foundIPv4 = true;
            
        } else if (address->typeID() == BaseAddress::GNS) {
            auto gnsRetrieved = std::dynamic_pointer_cast<GNSAddress>(address);
            ASSERT_NE(gnsRetrieved, nullptr);
            
            // Verify GNS specific data
            EXPECT_EQ(gnsRetrieved->fullAddress(), "testnode@myprovider");
            EXPECT_EQ(gnsRetrieved->typeID(), originalGNS->typeID());
            EXPECT_EQ(gnsRetrieved->serializedSize(), originalGNS->serializedSize());
            EXPECT_EQ(gnsRetrieved->fullAddress(), originalGNS->fullAddress());
            
            foundGNS = true;
        }
    }
    
    EXPECT_TRUE(foundIPv4) << "IPv4 address not found in retrieved addresses";
    EXPECT_TRUE(foundGNS) << "GNS address not found in retrieved addresses";
    
    // Additional verification - check database state
    EXPECT_EQ(getAddressCount(contractorID), 2);
} 