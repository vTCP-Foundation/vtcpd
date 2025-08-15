#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sqlite3.h>
#include <memory>
#include <string>
#include <sstream>
#include <filesystem>
#include <fstream>

#include "../../../src/core/io/storage/sqlite/AuditHandlerSQLite.h"
#include "../../../src/core/common/exceptions/IOError.h"
#include "../../../src/core/common/exceptions/NotFoundError.h"
#include "../../../src/core/common/exceptions/ValueError.h"
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/common/Types.h"

using namespace std;
using namespace testing;

// ------------------------------------------------------------------------------------------------
// Minimal stub factory to eliminate dependency on the obsolete TestDataFactory.
// It provides only the few helpers that are actually used in this test file.
class DummyFactory {
public:
    TrustLineID generateTrustLineID() {
        static TrustLineID sID = 1;
        return sID++;
    }

    TrustLineAmount generateTrustLineAmount() {
        return TrustLineAmount(100);
    }

    TrustLineBalance generateTrustLineBalance() {
        return TrustLineBalance(50);
    }
};

class AuditHandlerSQLiteTest : public Test {
protected:
    void SetUp() override {
        // Create temporary directory for test database
        tempDir = filesystem::temp_directory_path() / "audit_handler_test";
        filesystem::create_directories(tempDir);
        
        // Create test database
        testDbPath = tempDir / "test.db";
        int rc = sqlite3_open(testDbPath.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK);
        
        // Create Logger
        logger = make_unique<Logger>();
        
        // Create dummy data factory
        testDataFactory = make_unique<DummyFactory>();
        
        // Create handler
        handler = make_unique<AuditHandlerSQLite>(
            db, 
            "audit_test", 
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
    
    // Helper methods
    KeyHash::Shared generateTestKeyHash() {
        byte_t hashData[KeyHash::kBytesSize];
        std::fill(hashData, hashData + KeyHash::kBytesSize, 0xAB);
        return make_shared<KeyHash>(hashData);
    }
    
    Signature::Shared generateTestSignature() {
        byte_t sigBuf[Signature::signatureSize()];
        std::fill(sigBuf, sigBuf + Signature::signatureSize(), 0xCD);
        return make_shared<Signature>(sigBuf);
    }
    
    TrustLineAmount generateTestAmount() {
        return testDataFactory->generateTrustLineAmount();
    }
    
    TrustLineBalance generateTestBalance() {
        return testDataFactory->generateTrustLineBalance();
    }
    
    TrustLineID generateTestTrustLineID() {
        return testDataFactory->generateTrustLineID();
    }
    
    filesystem::path tempDir;
    filesystem::path testDbPath;
    sqlite3* db = nullptr;
    unique_ptr<Logger> logger;
    unique_ptr<DummyFactory> testDataFactory;
    unique_ptr<AuditHandlerSQLite> handler;
};

// Constructor Tests
TEST_F(AuditHandlerSQLiteTest, Constructor_ValidParameters_CreatesTableAndIndexes) {
    // Verify table exists
    string query = "SELECT name FROM sqlite_master WHERE type='table' AND name='audit_test';";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt);
    EXPECT_EQ(rc, SQLITE_ROW);
    
    string tableName = (char*)sqlite3_column_text(stmt, 0);
    EXPECT_EQ(tableName, "audit_test");
    
    sqlite3_finalize(stmt);
}

TEST_F(AuditHandlerSQLiteTest, Constructor_NullDatabase_ThrowsException) {
    EXPECT_THROW(
        AuditHandlerSQLite(nullptr, "test_table", *logger),
        std::exception
    );
}

TEST_F(AuditHandlerSQLiteTest, Constructor_EmptyTableName_ThrowsException) {
    EXPECT_THROW(
        AuditHandlerSQLite(db, "", *logger),
        std::exception
    );
}

// saveFullAudit Tests
TEST_F(AuditHandlerSQLiteTest, SaveFullAudit_ValidParameters_SavesSuccessfully) {
    AuditNumber number = 1;
    TrustLineID trustLineID = generateTestTrustLineID();
    KeyHash::Shared ownKeyHash = generateTestKeyHash();
    Signature::Shared ownSignature = generateTestSignature();
    KeyHash::Shared contractorKeyHash = generateTestKeyHash();
    Signature::Shared contractorSignature = generateTestSignature();
    KeyHash::Shared ownKeysSetHash = generateTestKeyHash();
    KeyHash::Shared contractorKeysSetHash = generateTestKeyHash();
    TrustLineAmount incomingAmount = generateTestAmount();
    TrustLineAmount outgoingAmount = generateTestAmount();
    TrustLineBalance balance = generateTestBalance();
    
    EXPECT_NO_THROW(
        handler->saveFullAudit(
            number, trustLineID, ownKeyHash, ownSignature,
            contractorKeyHash, contractorSignature,
            ownKeysSetHash, contractorKeysSetHash,
            incomingAmount, outgoingAmount, balance
        )
    );
    
    // Verify audit was saved
    AuditNumber actualNumber = handler->getActualAuditNumber(trustLineID);
    EXPECT_EQ(actualNumber, number);
}

TEST_F(AuditHandlerSQLiteTest, SaveFullAudit_NullOwnKeyHash_ThrowsException) {
    AuditNumber number = 1;
    TrustLineID trustLineID = generateTestTrustLineID();
    Signature::Shared ownSignature = generateTestSignature();
    KeyHash::Shared contractorKeyHash = generateTestKeyHash();
    Signature::Shared contractorSignature = generateTestSignature();
    KeyHash::Shared ownKeysSetHash = generateTestKeyHash();
    KeyHash::Shared contractorKeysSetHash = generateTestKeyHash();
    TrustLineAmount incomingAmount = generateTestAmount();
    TrustLineAmount outgoingAmount = generateTestAmount();
    TrustLineBalance balance = generateTestBalance();
    
    EXPECT_THROW(
        handler->saveFullAudit(
            number, trustLineID, nullptr, ownSignature,
            contractorKeyHash, contractorSignature,
            ownKeysSetHash, contractorKeysSetHash,
            incomingAmount, outgoingAmount, balance
        ),
        IOError
    );
}

// saveOwnAuditPart Tests
TEST_F(AuditHandlerSQLiteTest, SaveOwnAuditPart_ValidParameters_SavesSuccessfully) {
    AuditNumber number = 1;
    TrustLineID trustLineID = generateTestTrustLineID();
    KeyHash::Shared ownKeyHash = generateTestKeyHash();
    Signature::Shared ownSignature = generateTestSignature();
    KeyHash::Shared ownKeysSetHash = generateTestKeyHash();
    KeyHash::Shared contractorKeysSetHash = generateTestKeyHash();
    TrustLineAmount incomingAmount = generateTestAmount();
    TrustLineAmount outgoingAmount = generateTestAmount();
    TrustLineBalance balance = generateTestBalance();
    
    EXPECT_NO_THROW(
        handler->saveOwnAuditPart(
            number, trustLineID, ownKeyHash, ownSignature,
            ownKeysSetHash, contractorKeysSetHash,
            incomingAmount, outgoingAmount, balance
        )
    );
    
    // Verify audit was saved
    AuditRecord::Shared audit = handler->getActualAudit(trustLineID);
    EXPECT_NE(audit, nullptr);
}

// saveContractorAuditPart Tests
TEST_F(AuditHandlerSQLiteTest, SaveContractorAuditPart_ValidParameters_SavesSuccessfully) {
    AuditNumber number = 1;
    TrustLineID trustLineID = generateTestTrustLineID();
    
    // First save own audit part
    KeyHash::Shared ownKeyHash = generateTestKeyHash();
    Signature::Shared ownSignature = generateTestSignature();
    KeyHash::Shared ownKeysSetHash = generateTestKeyHash();
    KeyHash::Shared contractorKeysSetHash = generateTestKeyHash();
    TrustLineAmount incomingAmount = generateTestAmount();
    TrustLineAmount outgoingAmount = generateTestAmount();
    TrustLineBalance balance = generateTestBalance();
    
    handler->saveOwnAuditPart(
        number, trustLineID, ownKeyHash, ownSignature,
        ownKeysSetHash, contractorKeysSetHash,
        incomingAmount, outgoingAmount, balance
    );
    
    // Then save contractor part
    KeyHash::Shared contractorKeyHash = generateTestKeyHash();
    Signature::Shared contractorSignature = generateTestSignature();
    
    EXPECT_NO_THROW(
        handler->saveContractorAuditPart(number, trustLineID, contractorKeyHash, contractorSignature)
    );
    
    // Verify full audit exists
    AuditRecord::Shared fullAudit = handler->getActualAuditFull(trustLineID);
    EXPECT_NE(fullAudit, nullptr);
}

// getActualAudit Tests
TEST_F(AuditHandlerSQLiteTest, GetActualAudit_ExistingAudit_ReturnsAudit) {
    AuditNumber number = 1;
    TrustLineID trustLineID = generateTestTrustLineID();
    KeyHash::Shared ownKeyHash = generateTestKeyHash();
    Signature::Shared ownSignature = generateTestSignature();
    KeyHash::Shared ownKeysSetHash = generateTestKeyHash();
    KeyHash::Shared contractorKeysSetHash = generateTestKeyHash();
    TrustLineAmount incomingAmount = generateTestAmount();
    TrustLineAmount outgoingAmount = generateTestAmount();
    TrustLineBalance balance = generateTestBalance();
    
    // Save audit
    handler->saveOwnAuditPart(
        number, trustLineID, ownKeyHash, ownSignature,
        ownKeysSetHash, contractorKeysSetHash,
        incomingAmount, outgoingAmount, balance
    );
    
    // Retrieve audit
    AuditRecord::Shared audit = handler->getActualAudit(trustLineID);
    EXPECT_NE(audit, nullptr);
}

TEST_F(AuditHandlerSQLiteTest, GetActualAudit_NonExistentAudit_ThrowsNotFoundError) {
    TrustLineID trustLineID = generateTestTrustLineID();
    
    EXPECT_THROW(
        handler->getActualAudit(trustLineID),
        NotFoundError
    );
}

// getActualAuditFull Tests
TEST_F(AuditHandlerSQLiteTest, GetActualAuditFull_ExistingFullAudit_ReturnsAudit) {
    AuditNumber number = 1;
    TrustLineID trustLineID = generateTestTrustLineID();
    KeyHash::Shared ownKeyHash = generateTestKeyHash();
    Signature::Shared ownSignature = generateTestSignature();
    KeyHash::Shared contractorKeyHash = generateTestKeyHash();
    Signature::Shared contractorSignature = generateTestSignature();
    KeyHash::Shared ownKeysSetHash = generateTestKeyHash();
    KeyHash::Shared contractorKeysSetHash = generateTestKeyHash();
    TrustLineAmount incomingAmount = generateTestAmount();
    TrustLineAmount outgoingAmount = generateTestAmount();
    TrustLineBalance balance = generateTestBalance();
    
    // Save full audit
    handler->saveFullAudit(
        number, trustLineID, ownKeyHash, ownSignature,
        contractorKeyHash, contractorSignature,
        ownKeysSetHash, contractorKeysSetHash,
        incomingAmount, outgoingAmount, balance
    );
    
    // Retrieve full audit
    AuditRecord::Shared fullAudit = handler->getActualAuditFull(trustLineID);
    EXPECT_NE(fullAudit, nullptr);
}

TEST_F(AuditHandlerSQLiteTest, GetActualAuditFull_NonExistentAudit_ThrowsNotFoundError) {
    TrustLineID trustLineID = generateTestTrustLineID();
    
    EXPECT_THROW(
        handler->getActualAuditFull(trustLineID),
        NotFoundError
    );
}

// getActualAuditNumber Tests
TEST_F(AuditHandlerSQLiteTest, GetActualAuditNumber_ExistingAudit_ReturnsCorrectNumber) {
    AuditNumber number = 5;
    TrustLineID trustLineID = generateTestTrustLineID();
    KeyHash::Shared ownKeyHash = generateTestKeyHash();
    Signature::Shared ownSignature = generateTestSignature();
    KeyHash::Shared ownKeysSetHash = generateTestKeyHash();
    KeyHash::Shared contractorKeysSetHash = generateTestKeyHash();
    TrustLineAmount incomingAmount = generateTestAmount();
    TrustLineAmount outgoingAmount = generateTestAmount();
    TrustLineBalance balance = generateTestBalance();
    
    // Save audit
    handler->saveOwnAuditPart(
        number, trustLineID, ownKeyHash, ownSignature,
        ownKeysSetHash, contractorKeysSetHash,
        incomingAmount, outgoingAmount, balance
    );
    
    // Retrieve audit number
    AuditNumber actualNumber = handler->getActualAuditNumber(trustLineID);
    EXPECT_EQ(actualNumber, number);
}

TEST_F(AuditHandlerSQLiteTest, GetActualAuditNumber_NonExistentAudit_ThrowsNotFoundError) {
    TrustLineID trustLineID = generateTestTrustLineID();
    
    EXPECT_THROW(
        handler->getActualAuditNumber(trustLineID),
        NotFoundError
    );
}

// deleteRecords Tests
TEST_F(AuditHandlerSQLiteTest, DeleteRecords_ExistingAudits_DeletesSuccessfully) {
    AuditNumber number = 1;
    TrustLineID trustLineID = generateTestTrustLineID();
    KeyHash::Shared ownKeyHash = generateTestKeyHash();
    Signature::Shared ownSignature = generateTestSignature();
    KeyHash::Shared ownKeysSetHash = generateTestKeyHash();
    KeyHash::Shared contractorKeysSetHash = generateTestKeyHash();
    TrustLineAmount incomingAmount = generateTestAmount();
    TrustLineAmount outgoingAmount = generateTestAmount();
    TrustLineBalance balance = generateTestBalance();
    
    // Save audit
    handler->saveOwnAuditPart(
        number, trustLineID, ownKeyHash, ownSignature,
        ownKeysSetHash, contractorKeysSetHash,
        incomingAmount, outgoingAmount, balance
    );
    
    // Verify audit exists
    EXPECT_NO_THROW(handler->getActualAuditNumber(trustLineID));
    
    // Delete records
    EXPECT_NO_THROW(
        handler->deleteRecords(trustLineID)
    );
    
    // Verify audit is deleted
    EXPECT_THROW(
        handler->getActualAuditNumber(trustLineID),
        NotFoundError
    );
}

TEST_F(AuditHandlerSQLiteTest, DeleteRecords_NonExistentTrustLine_DoesNotThrow) {
    TrustLineID trustLineID = generateTestTrustLineID();
    
    EXPECT_NO_THROW(
        handler->deleteRecords(trustLineID)
    );
}

// deleteAuditByNumber Tests
TEST_F(AuditHandlerSQLiteTest, DeleteAuditByNumber_ExistingAudit_DeletesSuccessfully) {
    AuditNumber number = 1;
    TrustLineID trustLineID = generateTestTrustLineID();
    KeyHash::Shared ownKeyHash = generateTestKeyHash();
    Signature::Shared ownSignature = generateTestSignature();
    KeyHash::Shared ownKeysSetHash = generateTestKeyHash();
    KeyHash::Shared contractorKeysSetHash = generateTestKeyHash();
    TrustLineAmount incomingAmount = generateTestAmount();
    TrustLineAmount outgoingAmount = generateTestAmount();
    TrustLineBalance balance = generateTestBalance();
    
    // Save audit
    handler->saveOwnAuditPart(
        number, trustLineID, ownKeyHash, ownSignature,
        ownKeysSetHash, contractorKeysSetHash,
        incomingAmount, outgoingAmount, balance
    );
    
    // Delete specific audit
    EXPECT_NO_THROW(
        handler->deleteAuditByNumber(trustLineID, number)
    );
    
    // Verify audit is deleted
    EXPECT_THROW(
        handler->getActualAuditNumber(trustLineID),
        NotFoundError
    );
}

// isContainsKeyHash Tests
TEST_F(AuditHandlerSQLiteTest, IsContainsKeyHash_ExistingKeyHash_ReturnsTrue) {
    AuditNumber number = 1;
    TrustLineID trustLineID = generateTestTrustLineID();
    KeyHash::Shared ownKeyHash = generateTestKeyHash();
    Signature::Shared ownSignature = generateTestSignature();
    KeyHash::Shared ownKeysSetHash = generateTestKeyHash();
    KeyHash::Shared contractorKeysSetHash = generateTestKeyHash();
    TrustLineAmount incomingAmount = generateTestAmount();
    TrustLineAmount outgoingAmount = generateTestAmount();
    TrustLineBalance balance = generateTestBalance();
    
    // Save audit
    handler->saveOwnAuditPart(
        number, trustLineID, ownKeyHash, ownSignature,
        ownKeysSetHash, contractorKeysSetHash,
        incomingAmount, outgoingAmount, balance
    );
    
    // Check if key hash exists
    bool contains = handler->isContainsKeyHash(ownKeyHash);
    EXPECT_TRUE(contains);
}

TEST_F(AuditHandlerSQLiteTest, IsContainsKeyHash_NonExistentKeyHash_ReturnsFalse) {
    KeyHash::Shared keyHash = generateTestKeyHash();
    
    bool contains = handler->isContainsKeyHash(keyHash);
    EXPECT_FALSE(contains);
}

// Integration Tests
TEST_F(AuditHandlerSQLiteTest, Integration_FullAuditLifecycle_WorksCorrectly) {
    AuditNumber number = 1;
    TrustLineID trustLineID = generateTestTrustLineID();
    KeyHash::Shared ownKeyHash = generateTestKeyHash();
    Signature::Shared ownSignature = generateTestSignature();
    KeyHash::Shared contractorKeyHash = generateTestKeyHash();
    Signature::Shared contractorSignature = generateTestSignature();
    KeyHash::Shared ownKeysSetHash = generateTestKeyHash();
    KeyHash::Shared contractorKeysSetHash = generateTestKeyHash();
    TrustLineAmount incomingAmount = TrustLineAmount(100);
    TrustLineAmount outgoingAmount = TrustLineAmount(50);
    TrustLineBalance balance = TrustLineBalance(25);
    
    // Save full audit
    EXPECT_NO_THROW(
        handler->saveFullAudit(
            number, trustLineID, ownKeyHash, ownSignature,
            contractorKeyHash, contractorSignature,
            ownKeysSetHash, contractorKeysSetHash,
            incomingAmount, outgoingAmount, balance
        )
    );
    
    // Retrieve audit number
    AuditNumber actualNumber = handler->getActualAuditNumber(trustLineID);
    EXPECT_EQ(actualNumber, number);
    
    // Retrieve audit
    AuditRecord::Shared audit = handler->getActualAudit(trustLineID);
    EXPECT_NE(audit, nullptr);
    
    // Retrieve full audit
    AuditRecord::Shared fullAudit = handler->getActualAuditFull(trustLineID);
    EXPECT_NE(fullAudit, nullptr);
    
    // Check key hash existence
    EXPECT_TRUE(handler->isContainsKeyHash(ownKeyHash));
    EXPECT_TRUE(handler->isContainsKeyHash(contractorKeyHash));
    
    // Delete audit
    EXPECT_NO_THROW(
        handler->deleteRecords(trustLineID)
    );
    
    // Verify deletion
    EXPECT_THROW(handler->getActualAuditNumber(trustLineID), NotFoundError);
    EXPECT_FALSE(handler->isContainsKeyHash(ownKeyHash));
}

// Performance Tests
TEST_F(AuditHandlerSQLiteTest, Performance_MultipleAudits_CompletesInReasonableTime) {
    TrustLineID trustLineID = generateTestTrustLineID();
    const int numAudits = 10;
    
    auto start = chrono::high_resolution_clock::now();
    
    // Save multiple audits
    for (int i = 0; i < numAudits; ++i) {
        AuditNumber number = i + 1;
        KeyHash::Shared ownKeyHash = generateTestKeyHash();
        Signature::Shared ownSignature = generateTestSignature();
        KeyHash::Shared ownKeysSetHash = generateTestKeyHash();
        KeyHash::Shared contractorKeysSetHash = generateTestKeyHash();
        TrustLineAmount incomingAmount = generateTestAmount();
        TrustLineAmount outgoingAmount = generateTestAmount();
        TrustLineBalance balance = generateTestBalance();
        
        handler->saveOwnAuditPart(
            number, trustLineID, ownKeyHash, ownSignature,
            ownKeysSetHash, contractorKeysSetHash,
            incomingAmount, outgoingAmount, balance
        );
    }
    
    // Retrieve audits
    for (int i = 0; i < numAudits; ++i) {
        handler->getActualAudit(trustLineID);
    }
    
    // Delete all audits
    handler->deleteRecords(trustLineID);
    
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time (3 seconds for 10 audits)
    EXPECT_LT(duration.count(), 3000);
}

// Error Handling Tests
TEST_F(AuditHandlerSQLiteTest, ErrorHandling_CorruptedDatabase_ThrowsIOError) {
    // Close the database to simulate corruption
    sqlite3_close(db);
    db = nullptr;
    
    AuditNumber number = 1;
    TrustLineID trustLineID = generateTestTrustLineID();
    KeyHash::Shared ownKeyHash = generateTestKeyHash();
    Signature::Shared ownSignature = generateTestSignature();
    KeyHash::Shared ownKeysSetHash = generateTestKeyHash();
    KeyHash::Shared contractorKeysSetHash = generateTestKeyHash();
    TrustLineAmount incomingAmount = generateTestAmount();
    TrustLineAmount outgoingAmount = generateTestAmount();
    TrustLineBalance balance = generateTestBalance();
    
    // Operations should throw IOError
    EXPECT_THROW(
        handler->saveOwnAuditPart(
            number, trustLineID, ownKeyHash, ownSignature,
            ownKeysSetHash, contractorKeysSetHash,
            incomingAmount, outgoingAmount, balance
        ),
        IOError
    );
    
    EXPECT_THROW(
        handler->getActualAuditNumber(trustLineID),
        IOError
    );
} 