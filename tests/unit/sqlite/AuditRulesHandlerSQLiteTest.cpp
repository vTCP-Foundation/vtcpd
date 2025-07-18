#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sqlite3.h>
#include <memory>
#include <string>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <limits>
#include <chrono>

#include "../../../src/core/io/storage/sqlite/AuditRulesHandlerSQLite.h"
#include "../../../src/core/trust_lines/audit_rules/BaseAuditRule.h"
#include "../../../src/core/common/exceptions/IOError.h"
#include "../../../src/core/common/exceptions/NotFoundError.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/common/Types.h"

// -----------------------------------------------------------------------------
// Minimal stub factory that provides only the functionality required by tests.
class DummyFactory {
public:
    TrustLineID generateTrustLineID() {
        static TrustLineID sID = 1;
        return sID++;
    }
};

using namespace std;
using namespace testing;

class AuditRulesHandlerSQLiteTest : public Test {
protected:
    void SetUp() override {
        // Create temporary directory for test database
        tempDir = filesystem::temp_directory_path() / "audit_rules_test";
        filesystem::create_directories(tempDir);
        
        // Create test database
        testDbPath = tempDir / "test.db";
        int rc = sqlite3_open(testDbPath.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK);
        
        // Create Logger
        logger = make_unique<Logger>();
        
        // Create dummy factory
        testDataFactory = make_unique<DummyFactory>();
        
        // Create handler
        handler = make_unique<AuditRulesHandlerSQLite>(
            db, 
            "audit_rules_test", 
            *logger
        );
    }
    
    void TearDown() override {
        handler.reset();
        logger.reset();
        testDataFactory.reset();
        
        if (db) {
            sqlite3_close(db);
        }
        
        filesystem::remove_all(tempDir);
    }
    
    filesystem::path tempDir;
    filesystem::path testDbPath;
    sqlite3* db = nullptr;
    unique_ptr<Logger> logger;
    unique_ptr<DummyFactory> testDataFactory;
    unique_ptr<AuditRulesHandlerSQLite> handler;
};

// Constructor Tests
TEST_F(AuditRulesHandlerSQLiteTest, Constructor_ValidParameters_CreatesTableAndIndex) {
    // Verify table exists
    string query = "SELECT name FROM sqlite_master WHERE type='table' AND name='audit_rules_test';";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt);
    EXPECT_EQ(rc, SQLITE_ROW);
    
    string tableName = (char*)sqlite3_column_text(stmt, 0);
    EXPECT_EQ(tableName, "audit_rules_test");
    
    sqlite3_finalize(stmt);
    
    // Verify index exists
    query = "SELECT name FROM sqlite_master WHERE type='index' AND name='audit_rules_test_trust_line_id_idx';";
    rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt);
    EXPECT_EQ(rc, SQLITE_ROW);
    
    sqlite3_finalize(stmt);
}

TEST_F(AuditRulesHandlerSQLiteTest, Constructor_NullDatabase_ThrowsException) {
    EXPECT_THROW(
        AuditRulesHandlerSQLite(nullptr, "test_table", *logger),
        std::exception
    );
}

TEST_F(AuditRulesHandlerSQLiteTest, Constructor_EmptyTableName_ThrowsException) {
    EXPECT_THROW(
        AuditRulesHandlerSQLite(db, "", *logger),
        std::exception
    );
}

// saveRule Tests
TEST_F(AuditRulesHandlerSQLiteTest, SaveRule_ValidParameters_SavesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    BaseAuditRule::AuditRuleType ruleType = BaseAuditRule::AuditRuleCountPaymentsType;
    
    EXPECT_NO_THROW(
        handler->saveRule(trustLineID, ruleType)
    );
    
    // Verify saved data
    string query = "SELECT rule_id FROM audit_rules_test WHERE trust_line_id = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_bind_int(stmt, 1, trustLineID);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt);
    EXPECT_EQ(rc, SQLITE_ROW);
    
    int savedRuleType = sqlite3_column_int(stmt, 0);
    EXPECT_EQ(savedRuleType, ruleType);
    
    sqlite3_finalize(stmt);
}

TEST_F(AuditRulesHandlerSQLiteTest, SaveRule_AllRuleTypes_SavesCorrectly) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    // Test all rule types
    vector<BaseAuditRule::AuditRuleType> ruleTypes = {
        BaseAuditRule::AuditRuleCountPaymentsType,
        BaseAuditRule::AuditRuleTimeType,
        BaseAuditRule::AuditRuleTrustLineAmountBoundaryType
    };
    
    for (size_t i = 0; i < ruleTypes.size(); ++i) {
        TrustLineID currentTrustLineID = trustLineID + i;
        
        EXPECT_NO_THROW(
            handler->saveRule(currentTrustLineID, ruleTypes[i])
        );
        
        // Verify saved data
        BaseAuditRule::AuditRuleType retrievedType = handler->getRule(currentTrustLineID);
        EXPECT_EQ(retrievedType, ruleTypes[i]);
    }
}

TEST_F(AuditRulesHandlerSQLiteTest, SaveRule_DuplicateTrustLineID_ThrowsException) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    BaseAuditRule::AuditRuleType ruleType = BaseAuditRule::AuditRuleCountPaymentsType;
    
    // Save first rule
    EXPECT_NO_THROW(
        handler->saveRule(trustLineID, ruleType)
    );
    
    // Try to save duplicate
    EXPECT_THROW(
        handler->saveRule(trustLineID, BaseAuditRule::AuditRuleTimeType),
        IOError
    );
}

TEST_F(AuditRulesHandlerSQLiteTest, SaveRule_ZeroTrustLineID_SavesSuccessfully) {
    TrustLineID trustLineID = 0;
    BaseAuditRule::AuditRuleType ruleType = BaseAuditRule::AuditRuleCountPaymentsType;
    
    EXPECT_NO_THROW(
        handler->saveRule(trustLineID, ruleType)
    );
    
    BaseAuditRule::AuditRuleType retrievedType = handler->getRule(trustLineID);
    EXPECT_EQ(retrievedType, ruleType);
}

TEST_F(AuditRulesHandlerSQLiteTest, SaveRule_LargeTrustLineID_SavesSuccessfully) {
    TrustLineID trustLineID = numeric_limits<TrustLineID>::max();
    BaseAuditRule::AuditRuleType ruleType = BaseAuditRule::AuditRuleTrustLineAmountBoundaryType;
    
    EXPECT_NO_THROW(
        handler->saveRule(trustLineID, ruleType)
    );
    
    BaseAuditRule::AuditRuleType retrievedType = handler->getRule(trustLineID);
    EXPECT_EQ(retrievedType, ruleType);
}

// getRule Tests
TEST_F(AuditRulesHandlerSQLiteTest, GetRule_ExistingRule_ReturnsCorrectType) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    BaseAuditRule::AuditRuleType ruleType = BaseAuditRule::AuditRuleTimeType;
    
    handler->saveRule(trustLineID, ruleType);
    
    BaseAuditRule::AuditRuleType retrievedType = handler->getRule(trustLineID);
    EXPECT_EQ(retrievedType, ruleType);
}

TEST_F(AuditRulesHandlerSQLiteTest, GetRule_NonExistentRule_ThrowsNotFoundError) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_THROW(
        handler->getRule(trustLineID),
        NotFoundError
    );
}

TEST_F(AuditRulesHandlerSQLiteTest, GetRule_AfterRemoval_ThrowsNotFoundError) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    BaseAuditRule::AuditRuleType ruleType = BaseAuditRule::AuditRuleCountPaymentsType;
    
    handler->saveRule(trustLineID, ruleType);
    handler->removeAuditRules(trustLineID);
    
    EXPECT_THROW(
        handler->getRule(trustLineID),
        NotFoundError
    );
}

// removeAuditRules Tests
TEST_F(AuditRulesHandlerSQLiteTest, RemoveAuditRules_ExistingRule_RemovesSuccessfully) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    BaseAuditRule::AuditRuleType ruleType = BaseAuditRule::AuditRuleCountPaymentsType;
    
    handler->saveRule(trustLineID, ruleType);
    
    EXPECT_NO_THROW(
        handler->removeAuditRules(trustLineID)
    );
    
    // Verify removal
    EXPECT_THROW(
        handler->getRule(trustLineID),
        NotFoundError
    );
}

TEST_F(AuditRulesHandlerSQLiteTest, RemoveAuditRules_NonExistentRule_DoesNotThrow) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    
    EXPECT_NO_THROW(
        handler->removeAuditRules(trustLineID)
    );
}

TEST_F(AuditRulesHandlerSQLiteTest, RemoveAuditRules_MultipleCalls_DoesNotThrow) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    BaseAuditRule::AuditRuleType ruleType = BaseAuditRule::AuditRuleCountPaymentsType;
    
    handler->saveRule(trustLineID, ruleType);
    
    EXPECT_NO_THROW(handler->removeAuditRules(trustLineID));
    EXPECT_NO_THROW(handler->removeAuditRules(trustLineID));
    EXPECT_NO_THROW(handler->removeAuditRules(trustLineID));
}

// Integration Tests
TEST_F(AuditRulesHandlerSQLiteTest, Integration_SaveGetRemove_WorksCorrectly) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    BaseAuditRule::AuditRuleType ruleType = BaseAuditRule::AuditRuleTrustLineAmountBoundaryType;
    
    // Save
    EXPECT_NO_THROW(handler->saveRule(trustLineID, ruleType));
    
    // Get
    BaseAuditRule::AuditRuleType retrievedType = handler->getRule(trustLineID);
    EXPECT_EQ(retrievedType, ruleType);
    
    // Remove
    EXPECT_NO_THROW(handler->removeAuditRules(trustLineID));
    
    // Verify removal
    EXPECT_THROW(handler->getRule(trustLineID), NotFoundError);
}

TEST_F(AuditRulesHandlerSQLiteTest, Integration_MultipleRules_WorksCorrectly) {
    vector<TrustLineID> trustLineIDs;
    vector<BaseAuditRule::AuditRuleType> ruleTypes;
    
    // Generate test data
    for (int i = 0; i < 10; ++i) {
        trustLineIDs.push_back(testDataFactory->generateTrustLineID() + i);
        ruleTypes.push_back(
            static_cast<BaseAuditRule::AuditRuleType>(i % 3)
        );
    }
    
    // Save all rules
    for (size_t i = 0; i < trustLineIDs.size(); ++i) {
        EXPECT_NO_THROW(
            handler->saveRule(trustLineIDs[i], ruleTypes[i])
        );
    }
    
    // Verify all rules
    for (size_t i = 0; i < trustLineIDs.size(); ++i) {
        BaseAuditRule::AuditRuleType retrievedType = handler->getRule(trustLineIDs[i]);
        EXPECT_EQ(retrievedType, ruleTypes[i]);
    }
    
    // Remove half of the rules
    for (size_t i = 0; i < trustLineIDs.size() / 2; ++i) {
        EXPECT_NO_THROW(handler->removeAuditRules(trustLineIDs[i]));
    }
    
    // Verify removal
    for (size_t i = 0; i < trustLineIDs.size() / 2; ++i) {
        EXPECT_THROW(handler->getRule(trustLineIDs[i]), NotFoundError);
    }
    
    // Verify remaining rules
    for (size_t i = trustLineIDs.size() / 2; i < trustLineIDs.size(); ++i) {
        BaseAuditRule::AuditRuleType retrievedType = handler->getRule(trustLineIDs[i]);
        EXPECT_EQ(retrievedType, ruleTypes[i]);
    }
}

// Edge Case Tests
TEST_F(AuditRulesHandlerSQLiteTest, EdgeCase_SaveUpdatePattern_WorksCorrectly) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    BaseAuditRule::AuditRuleType ruleType1 = BaseAuditRule::AuditRuleCountPaymentsType;
    BaseAuditRule::AuditRuleType ruleType2 = BaseAuditRule::AuditRuleTimeType;
    
    // Save first rule
    EXPECT_NO_THROW(handler->saveRule(trustLineID, ruleType1));
    
    // Remove and save new rule (simulate update)
    EXPECT_NO_THROW(handler->removeAuditRules(trustLineID));
    EXPECT_NO_THROW(handler->saveRule(trustLineID, ruleType2));
    
    // Verify new rule
    BaseAuditRule::AuditRuleType retrievedType = handler->getRule(trustLineID);
    EXPECT_EQ(retrievedType, ruleType2);
}

TEST_F(AuditRulesHandlerSQLiteTest, EdgeCase_ConcurrentAccess_WorksCorrectly) {
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    BaseAuditRule::AuditRuleType ruleType = BaseAuditRule::AuditRuleCountPaymentsType;
    
    // Save rule
    EXPECT_NO_THROW(handler->saveRule(trustLineID, ruleType));
    
    // Multiple gets should work
    for (int i = 0; i < 5; ++i) {
        BaseAuditRule::AuditRuleType retrievedType = handler->getRule(trustLineID);
        EXPECT_EQ(retrievedType, ruleType);
    }
    
    // Remove should work
    EXPECT_NO_THROW(handler->removeAuditRules(trustLineID));
    
    // Multiple removes should not throw
    for (int i = 0; i < 5; ++i) {
        EXPECT_NO_THROW(handler->removeAuditRules(trustLineID));
    }
}

// Performance Tests
TEST_F(AuditRulesHandlerSQLiteTest, Performance_BulkOperations_CompletesInReasonableTime) {
    const int numRules = 1000;
    vector<TrustLineID> trustLineIDs;
    vector<BaseAuditRule::AuditRuleType> ruleTypes;
    
    // Generate test data
    for (int i = 0; i < numRules; ++i) {
        trustLineIDs.push_back(testDataFactory->generateTrustLineID() + i);
        ruleTypes.push_back(
            static_cast<BaseAuditRule::AuditRuleType>(i % 3)
        );
    }
    
    auto start = chrono::high_resolution_clock::now();
    
    // Bulk save
    for (size_t i = 0; i < trustLineIDs.size(); ++i) {
        handler->saveRule(trustLineIDs[i], ruleTypes[i]);
    }
    
    // Bulk get
    for (size_t i = 0; i < trustLineIDs.size(); ++i) {
        handler->getRule(trustLineIDs[i]);
    }
    
    // Bulk remove
    for (size_t i = 0; i < trustLineIDs.size(); ++i) {
        handler->removeAuditRules(trustLineIDs[i]);
    }
    
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time (30 seconds for 1000 operations)
    EXPECT_LT(duration.count(), 30000);
}

// Error Handling Tests
TEST_F(AuditRulesHandlerSQLiteTest, ErrorHandling_CorruptedDatabase_ThrowsIOError) {
    // Close the database to simulate corruption
    sqlite3_close(db);
    db = nullptr;
    
    TrustLineID trustLineID = testDataFactory->generateTrustLineID();
    BaseAuditRule::AuditRuleType ruleType = BaseAuditRule::AuditRuleCountPaymentsType;
    
    // Operations should throw IOError
    EXPECT_THROW(
        handler->saveRule(trustLineID, ruleType),
        IOError
    );
    
    EXPECT_THROW(
        handler->getRule(trustLineID),
        IOError
    );
    
    EXPECT_THROW(
        handler->removeAuditRules(trustLineID),
        IOError
    );
} 