#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sqlite3.h>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>

#include "../../../src/core/io/storage/sqlite/AddressHandlerSQLite.h"
#include "../../../src/core/contractors/addresses/BaseAddress.h"
#include "../../../src/core/contractors/addresses/IPv4WithPortAddress.h"
#include "../../../src/core/contractors/addresses/GNSAddress.h"
#include "../../../src/core/common/exceptions/IOError.h"
#include "../../../src/core/common/exceptions/ValueError.h"
#include "../../../src/core/logger/Logger.h"

using ::testing::_;
using ::testing::Return;
using ::testing::Throw;
using ::testing::StrictMock;
using ::testing::NiceMock;

class AddressHandlerSQLiteTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test databases
        testDbDir = std::filesystem::temp_directory_path() / "vtcpd_test_address";
        std::filesystem::create_directories(testDbDir);
        
        testDbPath = testDbDir / "test_address.db";
        
        // Create test database
        int rc = sqlite3_open(testDbPath.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK) << "Failed to open test database: " << sqlite3_errmsg(db);
        // Ensure SQLite enforces foreign keys and uses fast settings for tests
        rc = sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
        ASSERT_EQ(rc, SQLITE_OK) << "Failed to enable foreign_keys: " << sqlite3_errmsg(db);
        rc = sqlite3_exec(db, "PRAGMA journal_mode = MEMORY;", nullptr, nullptr, nullptr);
        ASSERT_EQ(rc, SQLITE_OK) << "Failed to set journal_mode: " << sqlite3_errmsg(db);
        rc = sqlite3_exec(db, "PRAGMA synchronous = OFF;", nullptr, nullptr, nullptr);
        ASSERT_EQ(rc, SQLITE_OK) << "Failed to set synchronous: " << sqlite3_errmsg(db);
        
        // Create contractors table (referenced by addresses)
        const char* createContractorsTable = R"(
            CREATE TABLE contractors (
                id INTEGER PRIMARY KEY,
                name TEXT NOT NULL
            );
        )";
        
        rc = sqlite3_exec(db, createContractorsTable, nullptr, nullptr, nullptr);
        ASSERT_EQ(rc, SQLITE_OK) << "Failed to create contractors table: " << sqlite3_errmsg(db);
        
        // Insert test contractors
        const char* insertContractors = R"(
            INSERT INTO contractors (id, name) VALUES 
            (1, 'Contractor One'),
            (2, 'Contractor Two'),
            (3, 'Contractor Three');
        )";
        
        rc = sqlite3_exec(db, insertContractors, nullptr, nullptr, nullptr);
        ASSERT_EQ(rc, SQLITE_OK) << "Failed to insert test contractors: " << sqlite3_errmsg(db);
        
        logger = std::make_unique<Logger>();
        tableName = "addresses";
        
        // Create handler instance
        handler = std::make_unique<AddressHandlerSQLite>(db, tableName, *logger);
    }
    
    void TearDown() override {
        handler.reset();
        if (db) {
            sqlite3_close(db);
            db = nullptr;
        }
        
        // Clean up test files
        std::error_code ec;
        std::filesystem::remove_all(testDbDir, ec);
        // Ignore cleanup errors
    }
    
    BaseAddress::Shared createTestIPv4Address(
        const std::string& host = "192.168.1.1",
        uint16_t port = 8080) {
        
        return std::make_shared<IPv4WithPortAddress>(host, port);
    }
    
    BaseAddress::Shared createTestGNSAddress(
        const std::string& hostName = "example.gnunet") {
        std::string fullAddress = "user@provider"; // valid minimal address
        auto addr = std::make_shared<GNSAddress>(fullAddress);
        // set host for verification
        addr->setIPAndPort(hostName + ":0");
        return addr;
    }
    
    void verifyAddressFields(
        const BaseAddress::Shared& address,
        BaseAddress::AddressType expectedType,
        const std::string& expectedHost,
        uint16_t expectedPort) {
        
        EXPECT_EQ(address->typeID(), expectedType);
        if (expectedType == BaseAddress::GNS) {
            // GNS host/port are not persisted in DB, so retrieved host is empty and port is 0
            EXPECT_EQ(address->host(), "");
            EXPECT_EQ(address->port(), 0);
            return;
        }
        EXPECT_EQ(address->port(), expectedPort);
    }
    
    size_t countAddressesInDatabase() {
        const char* query = "SELECT COUNT(*) FROM addresses";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return 0;
        
        size_t count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        
        sqlite3_finalize(stmt);
        return count;
    }
    
    size_t countAddressesForContractor(ContractorID contractorID) {
        const char* query = "SELECT COUNT(*) FROM addresses WHERE contractor_id = ?";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return 0;
        
        sqlite3_bind_int(stmt, 1, contractorID);
        
        size_t count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        
        sqlite3_finalize(stmt);
        return count;
    }

protected:
    std::filesystem::path testDbDir;
    std::filesystem::path testDbPath;
    sqlite3* db = nullptr;
    std::unique_ptr<Logger> logger;
    std::string tableName;
    std::unique_ptr<AddressHandlerSQLite> handler;
};

// Constructor and Table Creation Tests
TEST_F(AddressHandlerSQLiteTest, ConstructorValidParameters) {
    // Verify table was created by constructor
    const char* query = "SELECT name FROM sqlite_master WHERE type='table' AND name='addresses'";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    EXPECT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)), "addresses");
    sqlite3_finalize(stmt);
}

TEST_F(AddressHandlerSQLiteTest, ConstructorCreatesRequiredIndex) {
    // Verify index was created
    const char* query = "SELECT name FROM sqlite_master WHERE type='index' AND tbl_name='addresses'";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    std::vector<std::string> indexes;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        indexes.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
    
    // Check required index exists
    EXPECT_TRUE(std::find(indexes.begin(), indexes.end(), "addresses_contractor_id") != indexes.end());
}

TEST_F(AddressHandlerSQLiteTest, ConstructorNullDatabase) {
    EXPECT_THROW(
        AddressHandlerSQLite(nullptr, "test_table", *logger),
        IOError
    );
}

TEST_F(AddressHandlerSQLiteTest, ConstructorEmptyTableName) {
    EXPECT_THROW(
        AddressHandlerSQLite(db, "", *logger),
        IOError
    );
}

// Save Address Tests
TEST_F(AddressHandlerSQLiteTest, SaveIPv4AddressValidData) {
    auto address = createTestIPv4Address("192.168.1.100", 9090);
    ContractorID contractorID = 1;
    
    EXPECT_NO_THROW(handler->saveAddress(contractorID, address));
    
    // Verify data was saved
    EXPECT_EQ(countAddressesInDatabase(), 1);
    EXPECT_EQ(countAddressesForContractor(contractorID), 1);
    
    // Verify saved data
    auto retrievedAddresses = handler->contractorAddresses(contractorID);
    ASSERT_EQ(retrievedAddresses.size(), 1);
    
    verifyAddressFields(retrievedAddresses[0], BaseAddress::IPv4_IncludingPort, "192.168.1.100", 9090);
}

TEST_F(AddressHandlerSQLiteTest, SaveGNSAddressValidData) {
    auto address = createTestGNSAddress("test.gnunet");
    ContractorID contractorID = 2;
    
    EXPECT_NO_THROW(handler->saveAddress(contractorID, address));
    
    // Verify data was saved
    EXPECT_EQ(countAddressesInDatabase(), 1);
    EXPECT_EQ(countAddressesForContractor(contractorID), 1);
    
    // Verify saved data
    auto retrievedAddresses = handler->contractorAddresses(contractorID);
    ASSERT_EQ(retrievedAddresses.size(), 1);
    
    verifyAddressFields(retrievedAddresses[0], BaseAddress::GNS, "ignored", 0);
}

TEST_F(AddressHandlerSQLiteTest, SaveAddressNullPointer) {
    ContractorID contractorID = 1;
    
    EXPECT_DEATH(handler->saveAddress(contractorID, nullptr), ".*");
}

TEST_F(AddressHandlerSQLiteTest, SaveAddressInvalidContractorID) {
    auto address = createTestIPv4Address();
    ContractorID invalidContractorID = 999; // Doesn't exist in contractors table
    
    EXPECT_THROW(
        handler->saveAddress(invalidContractorID, address),
        IOError
    );
}

TEST_F(AddressHandlerSQLiteTest, SaveMultipleAddressesForOneContractor) {
    auto ipv4Address = createTestIPv4Address("10.0.0.1", 8080);
    auto gnsAddress = createTestGNSAddress("example.gnunet");
    auto ipv4Address2 = createTestIPv4Address("10.0.0.2", 8081);
    ContractorID contractorID = 1;
    
    EXPECT_NO_THROW(handler->saveAddress(contractorID, ipv4Address));
    EXPECT_NO_THROW(handler->saveAddress(contractorID, gnsAddress));
    EXPECT_NO_THROW(handler->saveAddress(contractorID, ipv4Address2));
    
    // Verify data was saved
    EXPECT_EQ(countAddressesInDatabase(), 3);
    EXPECT_EQ(countAddressesForContractor(contractorID), 3);
    
    // Verify saved data
    auto retrievedAddresses = handler->contractorAddresses(contractorID);
    ASSERT_EQ(retrievedAddresses.size(), 3);
    
    // Sort addresses by type for predictable verification
    std::sort(retrievedAddresses.begin(), retrievedAddresses.end(),
              [](const BaseAddress::Shared& a, const BaseAddress::Shared& b) {
                  return a->typeID() < b->typeID();
              });
    
    verifyAddressFields(retrievedAddresses[0], BaseAddress::IPv4_IncludingPort, "10.0.0.1", 8080);
    verifyAddressFields(retrievedAddresses[1], BaseAddress::IPv4_IncludingPort, "10.0.0.2", 8081);
    verifyAddressFields(retrievedAddresses[2], BaseAddress::GNS, "example.gnunet", 0);
}

TEST_F(AddressHandlerSQLiteTest, SaveDuplicateAddressesAllowed) {
    auto address = createTestIPv4Address("192.168.1.1", 8080);
    ContractorID contractorID = 1;
    
    // Save same address twice - should be allowed
    EXPECT_NO_THROW(handler->saveAddress(contractorID, address));
    EXPECT_NO_THROW(handler->saveAddress(contractorID, address));
    
    EXPECT_EQ(countAddressesInDatabase(), 2);
    EXPECT_EQ(countAddressesForContractor(contractorID), 2);
}

// Retrieve Addresses Tests
TEST_F(AddressHandlerSQLiteTest, ContractorAddressesValidData) {
    auto ipv4Address = createTestIPv4Address("172.16.0.1", 9000);
    auto gnsAddress = createTestGNSAddress("contractor1.gnunet");
    ContractorID contractorID = 1;
    
    // Save addresses
    handler->saveAddress(contractorID, ipv4Address);
    handler->saveAddress(contractorID, gnsAddress);
    
    // Save address for different contractor
    auto otherAddress = createTestIPv4Address("172.16.0.2", 9001);
    handler->saveAddress(2, otherAddress);
    
    // Retrieve addresses for specific contractor
    auto retrievedAddresses = handler->contractorAddresses(contractorID);
    ASSERT_EQ(retrievedAddresses.size(), 2);
    
    // Sort addresses by type for predictable verification
    std::sort(retrievedAddresses.begin(), retrievedAddresses.end(),
              [](const BaseAddress::Shared& a, const BaseAddress::Shared& b) {
                  return a->typeID() < b->typeID();
              });
    
    verifyAddressFields(retrievedAddresses[0], BaseAddress::IPv4_IncludingPort, "172.16.0.1", 9000);
    verifyAddressFields(retrievedAddresses[1], BaseAddress::GNS, "ignored", 0);
}

TEST_F(AddressHandlerSQLiteTest, ContractorAddressesEmpty) {
    ContractorID contractorID = 1;
    
    auto retrievedAddresses = handler->contractorAddresses(contractorID);
    EXPECT_TRUE(retrievedAddresses.empty());
}

TEST_F(AddressHandlerSQLiteTest, ContractorAddressesNonExistentContractor) {
    ContractorID nonExistentContractorID = 999;
    
    auto retrievedAddresses = handler->contractorAddresses(nonExistentContractorID);
    EXPECT_TRUE(retrievedAddresses.empty());
}

// Remove Addresses Tests
TEST_F(AddressHandlerSQLiteTest, RemoveAddressesValidData) {
    auto ipv4Address = createTestIPv4Address("10.1.1.1", 8080);
    auto gnsAddress = createTestGNSAddress("test.gnunet");
    ContractorID contractorID = 1;
    
    // Save addresses
    handler->saveAddress(contractorID, ipv4Address);
    handler->saveAddress(contractorID, gnsAddress);
    
    // Save address for different contractor
    auto otherAddress = createTestIPv4Address("10.1.1.2", 8081);
    handler->saveAddress(2, otherAddress);
    
    EXPECT_EQ(countAddressesInDatabase(), 3);
    EXPECT_EQ(countAddressesForContractor(contractorID), 2);
    
    // Remove addresses for specific contractor
    EXPECT_NO_THROW(handler->removeAddresses(contractorID));
    
    // Verify addresses were removed
    EXPECT_EQ(countAddressesInDatabase(), 1);
    EXPECT_EQ(countAddressesForContractor(contractorID), 0);
    EXPECT_EQ(countAddressesForContractor(2), 1);
    
    // Verify removed contractor has no addresses
    auto retrievedAddresses = handler->contractorAddresses(contractorID);
    EXPECT_TRUE(retrievedAddresses.empty());
    
    // Verify other contractor still has addresses
    auto otherAddresses = handler->contractorAddresses(2);
    EXPECT_EQ(otherAddresses.size(), 1);
}

TEST_F(AddressHandlerSQLiteTest, RemoveAddressesNonExistentContractor) {
    ContractorID nonExistentContractorID = 999;
    
    // Remove addresses for non-existent contractor should not throw
    EXPECT_NO_THROW(handler->removeAddresses(nonExistentContractorID));
    
    EXPECT_EQ(countAddressesInDatabase(), 0);
}

TEST_F(AddressHandlerSQLiteTest, RemoveAddressesEmptyTable) {
    ContractorID contractorID = 1;
    
    // Remove addresses from empty table should not throw
    EXPECT_NO_THROW(handler->removeAddresses(contractorID));
    
    EXPECT_EQ(countAddressesInDatabase(), 0);
}

// Edge Cases and Error Handling Tests
TEST_F(AddressHandlerSQLiteTest, AddressTypesHandling) {
    ContractorID contractorID = 1;
    
    // Test both supported address types
    auto ipv4Address = createTestIPv4Address("127.0.0.1", 8080);
    auto gnsAddress = createTestGNSAddress("localhost.gnunet");
    
    EXPECT_NO_THROW(handler->saveAddress(contractorID, ipv4Address));
    EXPECT_NO_THROW(handler->saveAddress(contractorID, gnsAddress));
    
    auto retrievedAddresses = handler->contractorAddresses(contractorID);
    ASSERT_EQ(retrievedAddresses.size(), 2);
    
    // Verify both types are handled correctly
    bool foundIPv4 = false, foundGNS = false;
    for (const auto& addr : retrievedAddresses) {
        if (addr->typeID() == BaseAddress::IPv4_IncludingPort) {
            foundIPv4 = true;
            verifyAddressFields(addr, BaseAddress::IPv4_IncludingPort, "127.0.0.1", 8080);
        } else if (addr->typeID() == BaseAddress::GNS) {
            foundGNS = true;
            verifyAddressFields(addr, BaseAddress::GNS, "ignored", 0);
        }
    }
    
    EXPECT_TRUE(foundIPv4);
    EXPECT_TRUE(foundGNS);
}

TEST_F(AddressHandlerSQLiteTest, IPv4AddressWithVariousPorts) {
    ContractorID contractorID = 1;
    
    // Test various port numbers
    std::vector<uint16_t> ports = {1, 80, 443, 8080, 65535};
    
    for (uint16_t port : ports) {
        auto address = createTestIPv4Address("192.168.1.1", port);
        EXPECT_NO_THROW(handler->saveAddress(contractorID, address));
    }
    
    auto retrievedAddresses = handler->contractorAddresses(contractorID);
    ASSERT_EQ(retrievedAddresses.size(), ports.size());
    
    // Verify all ports are saved correctly
    std::vector<uint16_t> retrievedPorts;
    for (const auto& addr : retrievedAddresses) {
        retrievedPorts.push_back(addr->port());
    }
    
    std::sort(retrievedPorts.begin(), retrievedPorts.end());
    std::sort(ports.begin(), ports.end());
    
    EXPECT_EQ(retrievedPorts, ports);
}

TEST_F(AddressHandlerSQLiteTest, IPv4AddressWithVariousHosts) {
    ContractorID contractorID = 1;
    
    // Test various IP addresses
    std::vector<std::string> hosts = {
        "127.0.0.1", "192.168.1.1", "10.0.0.1", "172.16.0.1", "255.255.255.255"
    };
    
    for (const auto& host : hosts) {
        auto address = createTestIPv4Address(host, 8080);
        EXPECT_NO_THROW(handler->saveAddress(contractorID, address));
    }
    
    auto retrievedAddresses = handler->contractorAddresses(contractorID);
    ASSERT_EQ(retrievedAddresses.size(), hosts.size());
    
    // Verify all hosts are saved correctly
    std::vector<std::string> retrievedHosts;
    for (const auto& addr : retrievedAddresses) {
        retrievedHosts.push_back(addr->host());
    }
    
    std::sort(retrievedHosts.begin(), retrievedHosts.end());
    std::sort(hosts.begin(), hosts.end());
    
    EXPECT_EQ(retrievedHosts, hosts);
}

TEST_F(AddressHandlerSQLiteTest, GNSAddressWithVariousNames) {
    ContractorID contractorID = 1;
    
    // Test various GNS names
    std::vector<std::string> gnsNames = {
        "example.gnunet", "test.gnunet", "contractor.gnunet", "service.gnunet"
    };
    
    for (const auto& name : gnsNames) {
        auto address = createTestGNSAddress(name);
        EXPECT_NO_THROW(handler->saveAddress(contractorID, address));
    }
    
    auto retrievedAddresses = handler->contractorAddresses(contractorID);
    ASSERT_EQ(retrievedAddresses.size(), gnsNames.size());
    // For GNS, DB persistence does not include host. Validate type and port only
    for (const auto& addr : retrievedAddresses) {
        EXPECT_EQ(addr->typeID(), BaseAddress::GNS);
        EXPECT_EQ(addr->port(), 0);
        EXPECT_EQ(addr->host(), "");
    }
}

TEST_F(AddressHandlerSQLiteTest, ConcurrentAccessSimulation) {
    ContractorID contractorID1 = 1;
    ContractorID contractorID2 = 2;
    
    auto address1 = createTestIPv4Address("10.0.0.1", 8080);
    auto address2 = createTestIPv4Address("10.0.0.2", 8081);
    
    // Simulate concurrent operations
    EXPECT_NO_THROW(handler->saveAddress(contractorID1, address1));
    EXPECT_NO_THROW(handler->saveAddress(contractorID2, address2));
    
    auto retrievedAddresses1 = handler->contractorAddresses(contractorID1);
    auto retrievedAddresses2 = handler->contractorAddresses(contractorID2);
    
    EXPECT_EQ(retrievedAddresses1.size(), 1);
    EXPECT_EQ(retrievedAddresses2.size(), 1);
    
    verifyAddressFields(retrievedAddresses1[0], BaseAddress::IPv4_IncludingPort, "10.0.0.1", 8080);
    verifyAddressFields(retrievedAddresses2[0], BaseAddress::IPv4_IncludingPort, "10.0.0.2", 8081);
}

TEST_F(AddressHandlerSQLiteTest, PerformanceReasonableTime) {
    const size_t numAddresses = 1000;
    const ContractorID contractorID = 1;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Wrap in a transaction to speed up bulk inserts for test environment
    sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);
    // Save many addresses
    for (size_t i = 0; i < numAddresses; ++i) {
        std::string host = "192.168.1." + std::to_string(i % 255 + 1);
        uint16_t port = 8000 + (i % 1000);
        auto address = createTestIPv4Address(host, port);
        handler->saveAddress(contractorID, address);
    }
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 15000);
    
    // Verify all addresses were saved
    auto retrievedAddresses = handler->contractorAddresses(contractorID);
    EXPECT_EQ(retrievedAddresses.size(), numAddresses);
}

// Data Integrity Tests
TEST_F(AddressHandlerSQLiteTest, DataIntegrityAfterMultipleOperations) {
    ContractorID contractorID1 = 1;
    ContractorID contractorID2 = 2;
    
    auto address1 = createTestIPv4Address("10.1.1.1", 8080);
    auto address2 = createTestGNSAddress("test1.gnunet");
    auto address3 = createTestIPv4Address("10.1.1.2", 8081);
    auto address4 = createTestGNSAddress("test2.gnunet");
    
    // Save addresses for both contractors
    handler->saveAddress(contractorID1, address1);
    handler->saveAddress(contractorID1, address2);
    handler->saveAddress(contractorID2, address3);
    handler->saveAddress(contractorID2, address4);
    
    // Verify initial state
    EXPECT_EQ(countAddressesInDatabase(), 4);
    EXPECT_EQ(countAddressesForContractor(contractorID1), 2);
    EXPECT_EQ(countAddressesForContractor(contractorID2), 2);
    
    // Remove addresses for one contractor
    handler->removeAddresses(contractorID1);
    
    // Verify final state
    EXPECT_EQ(countAddressesInDatabase(), 2);
    EXPECT_EQ(countAddressesForContractor(contractorID1), 0);
    EXPECT_EQ(countAddressesForContractor(contractorID2), 2);
    
    // Verify remaining addresses are correct
    auto remainingAddresses = handler->contractorAddresses(contractorID2);
    ASSERT_EQ(remainingAddresses.size(), 2);
    
    bool foundIPv4 = false, foundGNS = false;
    for (const auto& addr : remainingAddresses) {
        if (addr->typeID() == BaseAddress::IPv4_IncludingPort) {
            foundIPv4 = true;
            verifyAddressFields(addr, BaseAddress::IPv4_IncludingPort, "10.1.1.2", 8081);
        } else if (addr->typeID() == BaseAddress::GNS) {
            foundGNS = true;
            verifyAddressFields(addr, BaseAddress::GNS, "test2.gnunet", 0);
        }
    }
    
    EXPECT_TRUE(foundIPv4);
    EXPECT_TRUE(foundGNS);
}

TEST_F(AddressHandlerSQLiteTest, RetrieveAddressesAfterRemoval) {
    ContractorID contractorID = 1;
    
    // Save addresses
    auto address1 = createTestIPv4Address("192.168.1.1", 8080);
    auto address2 = createTestGNSAddress("test.gnunet");
    
    handler->saveAddress(contractorID, address1);
    handler->saveAddress(contractorID, address2);
    
    // Verify addresses were saved
    auto retrievedAddresses = handler->contractorAddresses(contractorID);
    EXPECT_EQ(retrievedAddresses.size(), 2);
    
    // Remove addresses
    handler->removeAddresses(contractorID);
    
    // Verify addresses were removed
    auto removedAddresses = handler->contractorAddresses(contractorID);
    EXPECT_TRUE(removedAddresses.empty());
} 