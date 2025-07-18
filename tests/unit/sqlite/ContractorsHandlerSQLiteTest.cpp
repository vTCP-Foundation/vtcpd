#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sqlite3.h>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <chrono>

#include "../../../src/core/io/storage/sqlite/ContractorsHandlerSQLite.h"
#include "../../../src/core/contractors/Contractor.h"
#include "../../../src/core/crypto/MsgEncryptor.h"
#include "../../../src/core/contractors/addresses/IPv4WithPortAddress.h"
#include "../../../src/core/contractors/addresses/GNSAddress.h"
#include "../../../src/core/common/exceptions/IOError.h"
#include "../../../src/core/common/exceptions/ValueError.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/common/Types.h"

using ::testing::_;
using ::testing::Return;
using ::testing::Throw;
using ::testing::StrictMock;
using ::testing::NiceMock;

class ContractorsHandlerSQLiteTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test databases
        testDbDir = std::filesystem::temp_directory_path() / "vtcpd_test_contractors";
        std::filesystem::create_directories(testDbDir);
        
        testDbPath = testDbDir / "test_contractors.db";
        
        // Create test database
        int rc = sqlite3_open(testDbPath.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK) << "Failed to open test database: " << sqlite3_errmsg(db);
        
        logger = std::make_unique<Logger>();
        tableName = "contractors";
        
        // Create handler instance
        handler = std::make_unique<ContractorsHandlerSQLite>(db, tableName, *logger);
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
    
    MsgEncryptor::KeyTrio::Shared createTestKeyTrio() {
        // Create a test key trio using the generator
        return MsgEncryptor::generateKeyTrio();
    }
    
    Contractor::Shared createTestContractor(
        ContractorID id = 1,
        ContractorID idOnContractorSide = 0,
        bool isConfirmed = false,
        MsgEncryptor::KeyTrio::Shared keyTrio = nullptr) {
        
        if (!keyTrio) {
            keyTrio = createTestKeyTrio();
        }
        
        return std::make_shared<Contractor>(id, idOnContractorSide, keyTrio, isConfirmed);
    }
    
    Contractor::Shared createTestContractorWithAddresses(
        ContractorID id = 1,
        const std::string& ipv4Host = "192.168.1.1",
        uint16_t port = 8080) {
        
        auto keyTrio = createTestKeyTrio();
        std::vector<BaseAddress::Shared> addresses;
        addresses.push_back(std::make_shared<IPv4WithPortAddress>(ipv4Host, port));
        
        return std::make_shared<Contractor>(id, addresses, keyTrio);
    }
    
    void verifyContractorFields(
        const Contractor::Shared& contractor,
        ContractorID expectedId,
        ContractorID expectedIdOnContractorSide,
        bool expectedIsConfirmed) {
        
        EXPECT_EQ(contractor->getID(), expectedId);
        EXPECT_EQ(contractor->ownIdOnContractorSide(), expectedIdOnContractorSide);
        EXPECT_EQ(contractor->isConfirmed(), expectedIsConfirmed);
        EXPECT_NE(contractor->cryptoKey(), nullptr);
    }
    
    size_t countContractorsInDatabase() {
        const char* query = "SELECT COUNT(*) FROM contractors";
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
    
    bool contractorExistsInDatabase(ContractorID id) {
        const char* query = "SELECT COUNT(*) FROM contractors WHERE id = ?";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return false;
        
        sqlite3_bind_int(stmt, 1, id);
        
        bool exists = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            exists = sqlite3_column_int(stmt, 0) > 0;
        }
        
        sqlite3_finalize(stmt);
        return exists;
    }

protected:
    std::filesystem::path testDbDir;
    std::filesystem::path testDbPath;
    sqlite3* db = nullptr;
    std::unique_ptr<Logger> logger;
    std::string tableName;
    std::unique_ptr<ContractorsHandlerSQLite> handler;
};

// Constructor and Table Creation Tests
TEST_F(ContractorsHandlerSQLiteTest, ConstructorValidParameters) {
    // Verify table was created by constructor
    const char* query = "SELECT name FROM sqlite_master WHERE type='table' AND name='contractors'";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    EXPECT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)), "contractors");
    sqlite3_finalize(stmt);
}

TEST_F(ContractorsHandlerSQLiteTest, ConstructorCreatesRequiredIndex) {
    // Verify index was created
    const char* query = "SELECT name FROM sqlite_master WHERE type='index' AND tbl_name='contractors'";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    std::vector<std::string> indexes;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        indexes.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
    
    // Check required index exists
    EXPECT_TRUE(std::find(indexes.begin(), indexes.end(), "contractors_id_idx") != indexes.end());
}

TEST_F(ContractorsHandlerSQLiteTest, ConstructorNullDatabase) {
    EXPECT_THROW(
        ContractorsHandlerSQLite(nullptr, "test_table", *logger),
        IOError
    );
}

TEST_F(ContractorsHandlerSQLiteTest, ConstructorEmptyTableName) {
    EXPECT_THROW(
        ContractorsHandlerSQLite(db, "", *logger),
        IOError
    );
}

// Save Contractor Tests
TEST_F(ContractorsHandlerSQLiteTest, SaveContractorValidData) {
    auto contractor = createTestContractor(1, 0, false);
    
    EXPECT_NO_THROW(handler->saveContractor(contractor));
    
    // Verify data was saved
    EXPECT_EQ(countContractorsInDatabase(), 1);
    EXPECT_TRUE(contractorExistsInDatabase(1));
    
    // Verify saved data
    auto retrievedContractors = handler->allContractors();
    ASSERT_EQ(retrievedContractors.size(), 1);
    
    verifyContractorFields(retrievedContractors[0], 1, 0, false);
}

TEST_F(ContractorsHandlerSQLiteTest, SaveContractorNullPointer) {
    EXPECT_THROW(
        handler->saveContractor(nullptr),
        IOError
    );
}

TEST_F(ContractorsHandlerSQLiteTest, SaveContractorDuplicateId) {
    auto contractor1 = createTestContractor(1, 0, false);
    auto contractor2 = createTestContractor(1, 0, false);
    
    EXPECT_NO_THROW(handler->saveContractor(contractor1));
    
    // Attempting to save duplicate ID should throw
    EXPECT_THROW(
        handler->saveContractor(contractor2),
        IOError
    );
}

// Save Contractor Full Tests
TEST_F(ContractorsHandlerSQLiteTest, SaveContractorFullValidData) {
    auto contractor = createTestContractor(1, 100, true);
    
    EXPECT_NO_THROW(handler->saveContractorFull(contractor));
    
    // Verify data was saved
    EXPECT_EQ(countContractorsInDatabase(), 1);
    EXPECT_TRUE(contractorExistsInDatabase(1));
    
    // Verify saved data
    auto retrievedContractors = handler->allContractors();
    ASSERT_EQ(retrievedContractors.size(), 1);
    
    verifyContractorFields(retrievedContractors[0], 1, 100, true);
}

TEST_F(ContractorsHandlerSQLiteTest, SaveContractorFullNullPointer) {
    EXPECT_THROW(
        handler->saveContractorFull(nullptr),
        IOError
    );
}

TEST_F(ContractorsHandlerSQLiteTest, SaveContractorFullDuplicateId) {
    auto contractor1 = createTestContractor(1, 100, true);
    auto contractor2 = createTestContractor(1, 200, true);
    
    EXPECT_NO_THROW(handler->saveContractorFull(contractor1));
    
    // Attempting to save duplicate ID should throw
    EXPECT_THROW(
        handler->saveContractorFull(contractor2),
        IOError
    );
}

// Save Confirmation Info Tests
TEST_F(ContractorsHandlerSQLiteTest, SaveConfirmationInfoValidData) {
    auto contractor = createTestContractor(1, 0, false);
    
    // Save contractor first
    handler->saveContractor(contractor);
    
    // Update with confirmation info
    contractor->setOwnIdOnContractorSide(100);
    contractor->confirm();
    
    EXPECT_NO_THROW(handler->saveConfirmationInfo(contractor));
    
    // Verify updated data
    auto retrievedContractors = handler->allContractors();
    ASSERT_EQ(retrievedContractors.size(), 1);
    
    verifyContractorFields(retrievedContractors[0], 1, 100, true);
}

TEST_F(ContractorsHandlerSQLiteTest, SaveConfirmationInfoNonExistentContractor) {
    auto contractor = createTestContractor(999, 100, true);
    
    // Update non-existent contractor should throw
    EXPECT_THROW(
        handler->saveConfirmationInfo(contractor),
        ValueError
    );
}

TEST_F(ContractorsHandlerSQLiteTest, SaveConfirmationInfoNullPointer) {
    EXPECT_THROW(
        handler->saveConfirmationInfo(nullptr),
        IOError
    );
}

// Update Crypto Key Tests
TEST_F(ContractorsHandlerSQLiteTest, UpdateCryptoKeyValidData) {
    auto contractor = createTestContractor(1, 0, false);
    
    // Save contractor first
    handler->saveContractor(contractor);
    
    // Update with new crypto key
    auto newKeyTrio = createTestKeyTrio();
    contractor->setCryptoKey(newKeyTrio);
    
    EXPECT_NO_THROW(handler->updateCryptoKey(contractor));
    
    // Verify updated data
    auto retrievedContractors = handler->allContractors();
    ASSERT_EQ(retrievedContractors.size(), 1);
    
    verifyContractorFields(retrievedContractors[0], 1, 0, true); // should be confirmed after update
}

TEST_F(ContractorsHandlerSQLiteTest, UpdateCryptoKeyNonExistentContractor) {
    auto contractor = createTestContractor(999, 0, false);
    
    // Update non-existent contractor should throw
    EXPECT_THROW(
        handler->updateCryptoKey(contractor),
        ValueError
    );
}

TEST_F(ContractorsHandlerSQLiteTest, UpdateCryptoKeyNullPointer) {
    EXPECT_THROW(
        handler->updateCryptoKey(nullptr),
        IOError
    );
}

// Update Channel ID Tests
TEST_F(ContractorsHandlerSQLiteTest, UpdateChannelIdOnContractorSideValidData) {
    auto contractor = createTestContractor(1, 0, false);
    
    // Save contractor first
    handler->saveContractor(contractor);
    
    // Update channel ID
    contractor->setOwnIdOnContractorSide(200);
    
    EXPECT_NO_THROW(handler->updateChannelIdOnContractorSide(contractor));
    
    // Verify updated data
    auto retrievedContractors = handler->allContractors();
    ASSERT_EQ(retrievedContractors.size(), 1);
    
    verifyContractorFields(retrievedContractors[0], 1, 200, false);
}

TEST_F(ContractorsHandlerSQLiteTest, UpdateChannelIdOnContractorSideNonExistentContractor) {
    auto contractor = createTestContractor(999, 200, false);
    
    // Update non-existent contractor should throw
    EXPECT_THROW(
        handler->updateChannelIdOnContractorSide(contractor),
        ValueError
    );
}

TEST_F(ContractorsHandlerSQLiteTest, UpdateChannelIdOnContractorSideNullPointer) {
    EXPECT_THROW(
        handler->updateChannelIdOnContractorSide(nullptr),
        IOError
    );
}

// Retrieve Contractors Tests
TEST_F(ContractorsHandlerSQLiteTest, AllContractorsValidData) {
    auto contractor1 = createTestContractor(1, 100, true);
    auto contractor2 = createTestContractor(2, 0, false);
    auto contractor3 = createTestContractor(3, 300, true);
    
    // Save contractors
    handler->saveContractorFull(contractor1);
    handler->saveContractor(contractor2);
    handler->saveContractorFull(contractor3);
    
    // Retrieve all contractors
    auto retrievedContractors = handler->allContractors();
    ASSERT_EQ(retrievedContractors.size(), 3);
    
    // Sort by ID for predictable verification
    std::sort(retrievedContractors.begin(), retrievedContractors.end(),
              [](const Contractor::Shared& a, const Contractor::Shared& b) {
                  return a->getID() < b->getID();
              });
    
    verifyContractorFields(retrievedContractors[0], 1, 100, true);
    verifyContractorFields(retrievedContractors[1], 2, 0, false);
    verifyContractorFields(retrievedContractors[2], 3, 300, true);
}

TEST_F(ContractorsHandlerSQLiteTest, AllContractorsEmpty) {
    auto retrievedContractors = handler->allContractors();
    EXPECT_TRUE(retrievedContractors.empty());
}

TEST_F(ContractorsHandlerSQLiteTest, AllIDsValidData) {
    auto contractor1 = createTestContractor(1, 100, true);
    auto contractor2 = createTestContractor(2, 0, false);
    auto contractor3 = createTestContractor(5, 500, true);
    
    // Save contractors
    handler->saveContractorFull(contractor1);
    handler->saveContractor(contractor2);
    handler->saveContractorFull(contractor3);
    
    // Retrieve all IDs
    auto retrievedIDs = handler->allIDs();
    ASSERT_EQ(retrievedIDs.size(), 3);
    
    // Sort for predictable verification
    std::sort(retrievedIDs.begin(), retrievedIDs.end());
    
    EXPECT_EQ(retrievedIDs[0], 1);
    EXPECT_EQ(retrievedIDs[1], 2);
    EXPECT_EQ(retrievedIDs[2], 5);
}

TEST_F(ContractorsHandlerSQLiteTest, AllIDsEmpty) {
    auto retrievedIDs = handler->allIDs();
    EXPECT_TRUE(retrievedIDs.empty());
}

// Remove Contractor Tests
TEST_F(ContractorsHandlerSQLiteTest, RemoveContractorValidData) {
    auto contractor1 = createTestContractor(1, 100, true);
    auto contractor2 = createTestContractor(2, 200, true);
    
    // Save contractors
    handler->saveContractorFull(contractor1);
    handler->saveContractorFull(contractor2);
    
    EXPECT_EQ(countContractorsInDatabase(), 2);
    
    // Remove one contractor
    EXPECT_NO_THROW(handler->removeContractor(1));
    
    // Verify contractor was removed
    EXPECT_EQ(countContractorsInDatabase(), 1);
    EXPECT_FALSE(contractorExistsInDatabase(1));
    EXPECT_TRUE(contractorExistsInDatabase(2));
    
    // Verify remaining contractor
    auto retrievedContractors = handler->allContractors();
    ASSERT_EQ(retrievedContractors.size(), 1);
    verifyContractorFields(retrievedContractors[0], 2, 200, true);
}

TEST_F(ContractorsHandlerSQLiteTest, RemoveContractorNonExistent) {
    ContractorID nonExistentId = 999;
    
    // Remove non-existent contractor should throw
    EXPECT_THROW(
        handler->removeContractor(nonExistentId),
        ValueError
    );
}

// Edge Cases and Error Handling Tests
TEST_F(ContractorsHandlerSQLiteTest, ContractorWithLargeIDs) {
    ContractorID largeId = std::numeric_limits<ContractorID>::max() - 1;
    ContractorID largeIdOnContractorSide = std::numeric_limits<ContractorID>::max() - 2;
    
    auto contractor = createTestContractor(largeId, largeIdOnContractorSide, true);
    
    EXPECT_NO_THROW(handler->saveContractorFull(contractor));
    
    auto retrievedContractors = handler->allContractors();
    ASSERT_EQ(retrievedContractors.size(), 1);
    
    verifyContractorFields(retrievedContractors[0], largeId, largeIdOnContractorSide, true);
}

TEST_F(ContractorsHandlerSQLiteTest, ContractorConfirmationStates) {
    auto contractor1 = createTestContractor(1, 0, false);
    auto contractor2 = createTestContractor(2, 0, true);
    
    // Save both contractors
    handler->saveContractor(contractor1);
    handler->saveContractorFull(contractor2);
    
    auto retrievedContractors = handler->allContractors();
    ASSERT_EQ(retrievedContractors.size(), 2);
    
    // Sort by ID for predictable verification
    std::sort(retrievedContractors.begin(), retrievedContractors.end(),
              [](const Contractor::Shared& a, const Contractor::Shared& b) {
                  return a->getID() < b->getID();
              });
    
    verifyContractorFields(retrievedContractors[0], 1, 0, false);
    verifyContractorFields(retrievedContractors[1], 2, 0, true);
}

TEST_F(ContractorsHandlerSQLiteTest, ConcurrentAccessSimulation) {
    auto contractor1 = createTestContractor(1, 100, true);
    auto contractor2 = createTestContractor(2, 200, true);
    
    // Simulate concurrent operations
    EXPECT_NO_THROW(handler->saveContractorFull(contractor1));
    EXPECT_NO_THROW(handler->saveContractorFull(contractor2));
    
    auto retrievedContractors = handler->allContractors();
    EXPECT_EQ(retrievedContractors.size(), 2);
    
    // Verify both contractors exist
    auto retrievedIDs = handler->allIDs();
    std::sort(retrievedIDs.begin(), retrievedIDs.end());
    
    EXPECT_EQ(retrievedIDs[0], 1);
    EXPECT_EQ(retrievedIDs[1], 2);
}

TEST_F(ContractorsHandlerSQLiteTest, PerformanceReasonableTime) {
    const size_t numContractors = 1000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Save many contractors
    for (size_t i = 0; i < numContractors; ++i) {
        auto contractor = createTestContractor(i + 1, (i + 1) * 10, i % 2 == 0);
        handler->saveContractorFull(contractor);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time (less than 15 seconds)
    EXPECT_LT(duration.count(), 15000);
    
    // Verify all contractors were saved
    auto retrievedContractors = handler->allContractors();
    EXPECT_EQ(retrievedContractors.size(), numContractors);
}

// Data Integrity Tests
TEST_F(ContractorsHandlerSQLiteTest, DataIntegrityAfterMultipleOperations) {
    auto contractor1 = createTestContractor(1, 0, false);
    auto contractor2 = createTestContractor(2, 0, false);
    
    // Save contractors
    handler->saveContractor(contractor1);
    handler->saveContractor(contractor2);
    
    // Update contractor1 with confirmation info
    contractor1->setOwnIdOnContractorSide(100);
    contractor1->confirm();
    handler->saveConfirmationInfo(contractor1);
    
    // Update contractor2 crypto key
    auto newKeyTrio = createTestKeyTrio();
    contractor2->setCryptoKey(newKeyTrio);
    handler->updateCryptoKey(contractor2);
    
    // Update contractor1 channel ID
    contractor1->setOwnIdOnContractorSide(150);
    handler->updateChannelIdOnContractorSide(contractor1);
    
    // Verify final state
    auto retrievedContractors = handler->allContractors();
    ASSERT_EQ(retrievedContractors.size(), 2);
    
    // Sort by ID for predictable verification
    std::sort(retrievedContractors.begin(), retrievedContractors.end(),
              [](const Contractor::Shared& a, const Contractor::Shared& b) {
                  return a->getID() < b->getID();
              });
    
    verifyContractorFields(retrievedContractors[0], 1, 150, true);
    verifyContractorFields(retrievedContractors[1], 2, 0, true);
}

TEST_F(ContractorsHandlerSQLiteTest, UpdateOperationsOnDeletedContractor) {
    auto contractor = createTestContractor(1, 0, false);
    
    // Save and then delete contractor
    handler->saveContractor(contractor);
    handler->removeContractor(1);
    
    // Attempt to update deleted contractor
    contractor->setOwnIdOnContractorSide(100);
    EXPECT_THROW(
        handler->saveConfirmationInfo(contractor),
        ValueError
    );
    
    auto newKeyTrio = createTestKeyTrio();
    contractor->setCryptoKey(newKeyTrio);
    EXPECT_THROW(
        handler->updateCryptoKey(contractor),
        ValueError
    );
    
    EXPECT_THROW(
        handler->updateChannelIdOnContractorSide(contractor),
        ValueError
    );
}

TEST_F(ContractorsHandlerSQLiteTest, CryptoKeyIntegrity) {
    auto contractor = createTestContractor(1, 0, false);
    
    // Save contractor
    handler->saveContractor(contractor);
    
    // Retrieve and verify crypto key is preserved
    auto retrievedContractors = handler->allContractors();
    ASSERT_EQ(retrievedContractors.size(), 1);
    
    auto retrievedContractor = retrievedContractors[0];
    EXPECT_NE(retrievedContractor->cryptoKey(), nullptr);
    
    // Verify crypto key can be used (basic check)
    std::vector<byte_t> buffer;
    EXPECT_NO_THROW(retrievedContractor->cryptoKey()->serialize(buffer));
} 