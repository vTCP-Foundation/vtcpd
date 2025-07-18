#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sqlite3.h>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>

#include "../../../src/core/io/storage/sqlite/TrustLineHandlerSQLite.h"
#include "../../../src/core/trust_lines/TrustLine.h"
#include "../../../src/core/common/exceptions/IOError.h"
#include "../../../src/core/common/exceptions/ValueError.h"
#include "../fixtures/TestDataFactory.h"

using ::testing::_;
using ::testing::Return;
using ::testing::Throw;
using ::testing::StrictMock;
using ::testing::NiceMock;

class TrustLineHandlerSQLiteTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test databases
        testDbDir = std::filesystem::temp_directory_path() / "vtcpd_test_trustline";
        std::filesystem::create_directories(testDbDir);
        
        testDbPath = testDbDir / "test_trustline.db";
        
        // Create test database
        int rc = sqlite3_open(testDbPath.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK) << "Failed to open test database: " << sqlite3_errmsg(db);
        
        // Create contractors table (referenced by trust lines)
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
        
        logger = std::make_unique<Logger>("TrustLineHandlerTest");
        tableName = "trust_lines";
        
        // Create handler instance
        handler = std::make_unique<TrustLineHandlerSQLite>(db, tableName, *logger);
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
    
    TrustLine::Shared createTestTrustLine(
        TrustLineID id = 1,
        ContractorID contractorID = 1,
        bool isContractorGateway = false,
        TrustLine::TrustLineState state = TrustLine::TrustLineState::Active) {
        
        return std::make_shared<TrustLine>(id, contractorID, isContractorGateway, state);
    }
    
    void verifyTrustLineFields(
        const TrustLine::Shared& trustLine,
        TrustLineID expectedId,
        ContractorID expectedContractorID,
        bool expectedIsContractorGateway,
        TrustLine::TrustLineState expectedState) {
        
        EXPECT_EQ(trustLine->trustLineID(), expectedId);
        EXPECT_EQ(trustLine->contractorID(), expectedContractorID);
        EXPECT_EQ(trustLine->isContractorGateway(), expectedIsContractorGateway);
        EXPECT_EQ(trustLine->state(), expectedState);
    }
    
    size_t countTrustLinesInDatabase() {
        const char* query = "SELECT COUNT(*) FROM trust_lines";
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

protected:
    std::filesystem::path testDbDir;
    std::filesystem::path testDbPath;
    sqlite3* db = nullptr;
    std::unique_ptr<Logger> logger;
    std::string tableName;
    std::unique_ptr<TrustLineHandlerSQLite> handler;
};

// Constructor and Table Creation Tests
TEST_F(TrustLineHandlerSQLiteTest, ConstructorValidParameters) {
    // Verify table was created by constructor
    const char* query = "SELECT name FROM sqlite_master WHERE type='table' AND name='trust_lines'";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    EXPECT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)), "trust_lines");
    sqlite3_finalize(stmt);
}

TEST_F(TrustLineHandlerSQLiteTest, ConstructorCreatesRequiredIndexes) {
    // Verify indexes were created
    const char* query = "SELECT name FROM sqlite_master WHERE type='index' AND tbl_name='trust_lines'";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    std::vector<std::string> indexes;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        indexes.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
    
    // Check required indexes exist
    EXPECT_TRUE(std::find(indexes.begin(), indexes.end(), "trust_lines_id_idx") != indexes.end());
    EXPECT_TRUE(std::find(indexes.begin(), indexes.end(), "trust_lines_equivalent_idx") != indexes.end());
    EXPECT_TRUE(std::find(indexes.begin(), indexes.end(), "trust_lines_contractor_id_equivalent_idx") != indexes.end());
}

TEST_F(TrustLineHandlerSQLiteTest, ConstructorNullDatabase) {
    EXPECT_THROW(
        TrustLineHandlerSQLite(nullptr, "test_table", *logger),
        IOError
    );
}

TEST_F(TrustLineHandlerSQLiteTest, ConstructorEmptyTableName) {
    EXPECT_THROW(
        TrustLineHandlerSQLite(db, "", *logger),
        IOError
    );
}

// Save Trust Line Tests
TEST_F(TrustLineHandlerSQLiteTest, SaveTrustLineValidData) {
    auto trustLine = createTestTrustLine(1, 1, false, TrustLine::TrustLineState::Active);
    SerializedEquivalent equivalent = 100;
    
    EXPECT_NO_THROW(handler->saveTrustLine(trustLine, equivalent));
    
    // Verify data was saved
    EXPECT_EQ(countTrustLinesInDatabase(), 1);
    
    // Verify saved data
    auto retrievedTrustLines = handler->allTrustLinesByEquivalent(equivalent);
    ASSERT_EQ(retrievedTrustLines.size(), 1);
    
    verifyTrustLineFields(retrievedTrustLines[0], 1, 1, false, TrustLine::TrustLineState::Active);
}

TEST_F(TrustLineHandlerSQLiteTest, SaveTrustLineGatewayFlag) {
    auto trustLine = createTestTrustLine(2, 2, true, TrustLine::TrustLineState::Init);
    SerializedEquivalent equivalent = 200;
    
    EXPECT_NO_THROW(handler->saveTrustLine(trustLine, equivalent));
    
    auto retrievedTrustLines = handler->allTrustLinesByEquivalent(equivalent);
    ASSERT_EQ(retrievedTrustLines.size(), 1);
    
    verifyTrustLineFields(retrievedTrustLines[0], 2, 2, true, TrustLine::TrustLineState::Init);
}

TEST_F(TrustLineHandlerSQLiteTest, SaveTrustLineNullPointer) {
    SerializedEquivalent equivalent = 100;
    
    EXPECT_THROW(
        handler->saveTrustLine(nullptr, equivalent),
        IOError
    );
}

TEST_F(TrustLineHandlerSQLiteTest, SaveTrustLineDuplicateKey) {
    auto trustLine1 = createTestTrustLine(1, 1, false, TrustLine::TrustLineState::Active);
    auto trustLine2 = createTestTrustLine(1, 1, false, TrustLine::TrustLineState::Archived);
    SerializedEquivalent equivalent = 100;
    
    EXPECT_NO_THROW(handler->saveTrustLine(trustLine1, equivalent));
    
    // Attempting to save duplicate should throw
    EXPECT_THROW(
        handler->saveTrustLine(trustLine2, equivalent),
        IOError
    );
}

TEST_F(TrustLineHandlerSQLiteTest, SaveTrustLineInvalidContractorID) {
    // Contractor ID 999 doesn't exist in contractors table
    auto trustLine = createTestTrustLine(1, 999, false, TrustLine::TrustLineState::Active);
    SerializedEquivalent equivalent = 100;
    
    EXPECT_THROW(
        handler->saveTrustLine(trustLine, equivalent),
        IOError
    );
}

// Update Trust Line State Tests
TEST_F(TrustLineHandlerSQLiteTest, UpdateTrustLineStateValid) {
    auto trustLine = createTestTrustLine(1, 1, false, TrustLine::TrustLineState::Active);
    SerializedEquivalent equivalent = 100;
    
    // Save initial trust line
    handler->saveTrustLine(trustLine, equivalent);
    
    // Update state
    trustLine->setState(TrustLine::TrustLineState::Archived);
    EXPECT_NO_THROW(handler->updateTrustLineState(trustLine, equivalent));
    
    // Verify state was updated
    auto retrievedTrustLines = handler->allTrustLinesByEquivalent(equivalent);
    ASSERT_EQ(retrievedTrustLines.size(), 1);
    EXPECT_EQ(retrievedTrustLines[0]->state(), TrustLine::TrustLineState::Archived);
}

TEST_F(TrustLineHandlerSQLiteTest, UpdateTrustLineStateNonExistent) {
    auto trustLine = createTestTrustLine(999, 1, false, TrustLine::TrustLineState::Active);
    SerializedEquivalent equivalent = 100;
    
    // Update non-existent trust line should throw
    EXPECT_THROW(
        handler->updateTrustLineState(trustLine, equivalent),
        ValueError
    );
}

TEST_F(TrustLineHandlerSQLiteTest, UpdateTrustLineStateNullPointer) {
    SerializedEquivalent equivalent = 100;
    
    EXPECT_THROW(
        handler->updateTrustLineState(nullptr, equivalent),
        IOError
    );
}

// Update Trust Line Gateway Tests
TEST_F(TrustLineHandlerSQLiteTest, UpdateTrustLineGatewayValid) {
    auto trustLine = createTestTrustLine(1, 1, false, TrustLine::TrustLineState::Active);
    SerializedEquivalent equivalent = 100;
    
    // Save initial trust line
    handler->saveTrustLine(trustLine, equivalent);
    
    // Update gateway flag
    trustLine->setContractorAsGateway(true);
    EXPECT_NO_THROW(handler->updateTrustLineIsContractorGateway(trustLine, equivalent));
    
    // Verify gateway flag was updated
    auto retrievedTrustLines = handler->allTrustLinesByEquivalent(equivalent);
    ASSERT_EQ(retrievedTrustLines.size(), 1);
    EXPECT_TRUE(retrievedTrustLines[0]->isContractorGateway());
}

TEST_F(TrustLineHandlerSQLiteTest, UpdateTrustLineGatewayNonExistent) {
    auto trustLine = createTestTrustLine(999, 1, false, TrustLine::TrustLineState::Active);
    SerializedEquivalent equivalent = 100;
    
    // Update non-existent trust line should throw
    EXPECT_THROW(
        handler->updateTrustLineIsContractorGateway(trustLine, equivalent),
        ValueError
    );
}

TEST_F(TrustLineHandlerSQLiteTest, UpdateTrustLineGatewayNullPointer) {
    SerializedEquivalent equivalent = 100;
    
    EXPECT_THROW(
        handler->updateTrustLineIsContractorGateway(nullptr, equivalent),
        IOError
    );
}

// Retrieve Trust Lines Tests
TEST_F(TrustLineHandlerSQLiteTest, AllTrustLinesByEquivalentValid) {
    SerializedEquivalent equivalent = 100;
    
    // Save multiple trust lines with same equivalent
    auto trustLine1 = createTestTrustLine(1, 1, false, TrustLine::TrustLineState::Active);
    auto trustLine2 = createTestTrustLine(2, 2, true, TrustLine::TrustLineState::Init);
    
    handler->saveTrustLine(trustLine1, equivalent);
    handler->saveTrustLine(trustLine2, equivalent);
    
    // Save trust line with different equivalent
    auto trustLine3 = createTestTrustLine(3, 3, false, TrustLine::TrustLineState::Archived);
    handler->saveTrustLine(trustLine3, 200);
    
    // Retrieve by equivalent
    auto retrievedTrustLines = handler->allTrustLinesByEquivalent(equivalent);
    ASSERT_EQ(retrievedTrustLines.size(), 2);
    
    // Verify retrieved data
    std::sort(retrievedTrustLines.begin(), retrievedTrustLines.end(),
              [](const TrustLine::Shared& a, const TrustLine::Shared& b) {
                  return a->trustLineID() < b->trustLineID();
              });
    
    verifyTrustLineFields(retrievedTrustLines[0], 1, 1, false, TrustLine::TrustLineState::Active);
    verifyTrustLineFields(retrievedTrustLines[1], 2, 2, true, TrustLine::TrustLineState::Init);
}

TEST_F(TrustLineHandlerSQLiteTest, AllTrustLinesByEquivalentEmpty) {
    SerializedEquivalent equivalent = 999;
    
    auto retrievedTrustLines = handler->allTrustLinesByEquivalent(equivalent);
    EXPECT_TRUE(retrievedTrustLines.empty());
}

TEST_F(TrustLineHandlerSQLiteTest, AllTrustLinesByContractorValid) {
    ContractorID contractorID = 1;
    
    // Save multiple trust lines for same contractor
    auto trustLine1 = createTestTrustLine(1, contractorID, false, TrustLine::TrustLineState::Active);
    auto trustLine2 = createTestTrustLine(2, contractorID, true, TrustLine::TrustLineState::Init);
    
    handler->saveTrustLine(trustLine1, 100);
    handler->saveTrustLine(trustLine2, 200);
    
    // Save trust line for different contractor
    auto trustLine3 = createTestTrustLine(3, 2, false, TrustLine::TrustLineState::Archived);
    handler->saveTrustLine(trustLine3, 300);
    
    // Retrieve by contractor
    auto retrievedTrustLines = handler->allTrustLinesByContractor(contractorID);
    ASSERT_EQ(retrievedTrustLines.size(), 2);
    
    // Verify retrieved data
    std::sort(retrievedTrustLines.begin(), retrievedTrustLines.end(),
              [](const TrustLine::Shared& a, const TrustLine::Shared& b) {
                  return a->trustLineID() < b->trustLineID();
              });
    
    verifyTrustLineFields(retrievedTrustLines[0], 1, contractorID, false, TrustLine::TrustLineState::Active);
    verifyTrustLineFields(retrievedTrustLines[1], 2, contractorID, true, TrustLine::TrustLineState::Init);
}

TEST_F(TrustLineHandlerSQLiteTest, AllTrustLinesByContractorEmpty) {
    ContractorID contractorID = 999;
    
    auto retrievedTrustLines = handler->allTrustLinesByContractor(contractorID);
    EXPECT_TRUE(retrievedTrustLines.empty());
}

// Delete Trust Line Tests
TEST_F(TrustLineHandlerSQLiteTest, DeleteTrustLineValid) {
    auto trustLine = createTestTrustLine(1, 1, false, TrustLine::TrustLineState::Active);
    SerializedEquivalent equivalent = 100;
    
    // Save trust line
    handler->saveTrustLine(trustLine, equivalent);
    EXPECT_EQ(countTrustLinesInDatabase(), 1);
    
    // Delete trust line
    EXPECT_NO_THROW(handler->deleteTrustLine(1, equivalent));
    EXPECT_EQ(countTrustLinesInDatabase(), 0);
}

TEST_F(TrustLineHandlerSQLiteTest, DeleteTrustLineNonExistent) {
    // Delete non-existent trust line should not throw
    EXPECT_NO_THROW(handler->deleteTrustLine(999, 999));
}

TEST_F(TrustLineHandlerSQLiteTest, DeleteTrustLineSpecificEquivalent) {
    auto trustLine1 = createTestTrustLine(1, 1, false, TrustLine::TrustLineState::Active);
    auto trustLine2 = createTestTrustLine(2, 1, false, TrustLine::TrustLineState::Active);
    
    // Save trust lines with different equivalents
    handler->saveTrustLine(trustLine1, 100);
    handler->saveTrustLine(trustLine2, 200);
    EXPECT_EQ(countTrustLinesInDatabase(), 2);
    
    // Delete trust line with specific equivalent
    EXPECT_NO_THROW(handler->deleteTrustLine(1, 100));
    EXPECT_EQ(countTrustLinesInDatabase(), 1);
    
    // Verify correct trust line was deleted
    auto remainingTrustLines = handler->allTrustLinesByEquivalent(200);
    ASSERT_EQ(remainingTrustLines.size(), 1);
    EXPECT_EQ(remainingTrustLines[0]->trustLineID(), 2);
}

// Equivalents and IDs Tests
TEST_F(TrustLineHandlerSQLiteTest, EquivalentsValid) {
    // Save trust lines with different equivalents
    auto trustLine1 = createTestTrustLine(1, 1, false, TrustLine::TrustLineState::Active);
    auto trustLine2 = createTestTrustLine(2, 2, false, TrustLine::TrustLineState::Active);
    auto trustLine3 = createTestTrustLine(3, 3, false, TrustLine::TrustLineState::Active);
    
    handler->saveTrustLine(trustLine1, 100);
    handler->saveTrustLine(trustLine2, 200);
    handler->saveTrustLine(trustLine3, 100); // Duplicate equivalent
    
    auto equivalents = handler->equivalents();
    std::sort(equivalents.begin(), equivalents.end());
    
    ASSERT_EQ(equivalents.size(), 2);
    EXPECT_EQ(equivalents[0], 100);
    EXPECT_EQ(equivalents[1], 200);
}

TEST_F(TrustLineHandlerSQLiteTest, EquivalentsEmpty) {
    auto equivalents = handler->equivalents();
    EXPECT_TRUE(equivalents.empty());
}

TEST_F(TrustLineHandlerSQLiteTest, AllIDsValid) {
    // Save trust lines
    auto trustLine1 = createTestTrustLine(1, 1, false, TrustLine::TrustLineState::Active);
    auto trustLine2 = createTestTrustLine(2, 2, false, TrustLine::TrustLineState::Active);
    auto trustLine3 = createTestTrustLine(3, 3, false, TrustLine::TrustLineState::Active);
    
    handler->saveTrustLine(trustLine1, 100);
    handler->saveTrustLine(trustLine2, 200);
    handler->saveTrustLine(trustLine3, 300);
    
    auto ids = handler->allIDs();
    std::sort(ids.begin(), ids.end());
    
    ASSERT_EQ(ids.size(), 3);
    EXPECT_EQ(ids[0], 1);
    EXPECT_EQ(ids[1], 2);
    EXPECT_EQ(ids[2], 3);
}

TEST_F(TrustLineHandlerSQLiteTest, AllIDsEmpty) {
    auto ids = handler->allIDs();
    EXPECT_TRUE(ids.empty());
}

// Edge Cases and Error Handling Tests
TEST_F(TrustLineHandlerSQLiteTest, TrustLineStateEnumValues) {
    // Test all trust line states
    std::vector<TrustLine::TrustLineState> states = {
        TrustLine::TrustLineState::Init,
        TrustLine::TrustLineState::Active,
        TrustLine::TrustLineState::AuditPending,
        TrustLine::TrustLineState::Archived,
        TrustLine::TrustLineState::Conflict,
        TrustLine::TrustLineState::ConflictResolving,
        TrustLine::TrustLineState::ResetPending,
        TrustLine::TrustLineState::Reset,
        TrustLine::TrustLineState::KeysSharing
    };
    
    for (size_t i = 0; i < states.size(); ++i) {
        auto trustLine = createTestTrustLine(i + 1, 1, false, states[i]);
        SerializedEquivalent equivalent = 100 + i;
        
        EXPECT_NO_THROW(handler->saveTrustLine(trustLine, equivalent));
        
        auto retrievedTrustLines = handler->allTrustLinesByEquivalent(equivalent);
        ASSERT_EQ(retrievedTrustLines.size(), 1);
        EXPECT_EQ(retrievedTrustLines[0]->state(), states[i]);
    }
}

TEST_F(TrustLineHandlerSQLiteTest, LargeEquivalentValues) {
    auto trustLine = createTestTrustLine(1, 1, false, TrustLine::TrustLineState::Active);
    SerializedEquivalent largeEquivalent = std::numeric_limits<SerializedEquivalent>::max();
    
    EXPECT_NO_THROW(handler->saveTrustLine(trustLine, largeEquivalent));
    
    auto retrievedTrustLines = handler->allTrustLinesByEquivalent(largeEquivalent);
    ASSERT_EQ(retrievedTrustLines.size(), 1);
    verifyTrustLineFields(retrievedTrustLines[0], 1, 1, false, TrustLine::TrustLineState::Active);
}

TEST_F(TrustLineHandlerSQLiteTest, ConcurrentAccessSimulation) {
    auto trustLine1 = createTestTrustLine(1, 1, false, TrustLine::TrustLineState::Active);
    auto trustLine2 = createTestTrustLine(2, 2, false, TrustLine::TrustLineState::Active);
    
    // Simulate concurrent operations
    EXPECT_NO_THROW(handler->saveTrustLine(trustLine1, 100));
    EXPECT_NO_THROW(handler->saveTrustLine(trustLine2, 200));
    
    auto retrievedTrustLines1 = handler->allTrustLinesByEquivalent(100);
    auto retrievedTrustLines2 = handler->allTrustLinesByEquivalent(200);
    
    EXPECT_EQ(retrievedTrustLines1.size(), 1);
    EXPECT_EQ(retrievedTrustLines2.size(), 1);
}

TEST_F(TrustLineHandlerSQLiteTest, PerformanceReasonableTime) {
    const size_t numTrustLines = 1000;
    const SerializedEquivalent equivalent = 100;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Save many trust lines
    for (size_t i = 0; i < numTrustLines; ++i) {
        // Use different contractor IDs to avoid foreign key constraints
        ContractorID contractorID = (i % 3) + 1; // Use contractors 1, 2, 3
        auto trustLine = createTestTrustLine(i + 1, contractorID, false, TrustLine::TrustLineState::Active);
        handler->saveTrustLine(trustLine, equivalent);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time (less than 5 seconds)
    EXPECT_LT(duration.count(), 5000);
    
    // Verify all trust lines were saved
    auto retrievedTrustLines = handler->allTrustLinesByEquivalent(equivalent);
    EXPECT_EQ(retrievedTrustLines.size(), numTrustLines);
}

// Data Integrity Tests
TEST_F(TrustLineHandlerSQLiteTest, DataIntegrityAfterMultipleOperations) {
    auto trustLine1 = createTestTrustLine(1, 1, false, TrustLine::TrustLineState::Active);
    auto trustLine2 = createTestTrustLine(2, 2, false, TrustLine::TrustLineState::Init);
    
    // Save trust lines
    handler->saveTrustLine(trustLine1, 100);
    handler->saveTrustLine(trustLine2, 200);
    
    // Update states
    trustLine1->setState(TrustLine::TrustLineState::Archived);
    trustLine2->setState(TrustLine::TrustLineState::Active);
    
    handler->updateTrustLineState(trustLine1, 100);
    handler->updateTrustLineState(trustLine2, 200);
    
    // Update gateway flags
    trustLine1->setContractorAsGateway(true);
    trustLine2->setContractorAsGateway(true);
    
    handler->updateTrustLineIsContractorGateway(trustLine1, 100);
    handler->updateTrustLineIsContractorGateway(trustLine2, 200);
    
    // Verify final state
    auto retrievedTrustLines1 = handler->allTrustLinesByEquivalent(100);
    auto retrievedTrustLines2 = handler->allTrustLinesByEquivalent(200);
    
    ASSERT_EQ(retrievedTrustLines1.size(), 1);
    ASSERT_EQ(retrievedTrustLines2.size(), 1);
    
    verifyTrustLineFields(retrievedTrustLines1[0], 1, 1, true, TrustLine::TrustLineState::Archived);
    verifyTrustLineFields(retrievedTrustLines2[0], 2, 2, true, TrustLine::TrustLineState::Active);
}

TEST_F(TrustLineHandlerSQLiteTest, UpdateOperationsOnDeletedTrustLine) {
    auto trustLine = createTestTrustLine(1, 1, false, TrustLine::TrustLineState::Active);
    SerializedEquivalent equivalent = 100;
    
    // Save and then delete trust line
    handler->saveTrustLine(trustLine, equivalent);
    handler->deleteTrustLine(1, equivalent);
    
    // Attempt to update deleted trust line
    trustLine->setState(TrustLine::TrustLineState::Archived);
    EXPECT_THROW(
        handler->updateTrustLineState(trustLine, equivalent),
        ValueError
    );
    
    trustLine->setContractorAsGateway(true);
    EXPECT_THROW(
        handler->updateTrustLineIsContractorGateway(trustLine, equivalent),
        ValueError
    );
} 