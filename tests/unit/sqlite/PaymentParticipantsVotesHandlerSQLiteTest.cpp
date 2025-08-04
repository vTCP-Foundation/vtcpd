#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sqlite3.h>
#include <memory>
#include <string>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <map>

#include "../../../src/core/io/storage/sqlite/PaymentParticipantsVotesHandlerSQLite.h"
#include "../../../src/core/common/exceptions/IOError.h"
#include "../../../src/core/common/exceptions/NotFoundError.h"
#include "../../../src/core/common/exceptions/ValueError.h"
#include "../../../src/core/crypto/lamportkeys.h"
#include "../../../src/core/contractors/Contractor.h"
#include "../../../src/core/logger/Logger.h"
#include "../fixtures/TestDataFactory.h"

using namespace std;
using namespace testing;
using namespace crypto::lamport;

class PaymentParticipantsVotesHandlerSQLiteTest : public Test {
protected:
    void SetUp() override {
        // Create temporary directory for test database
        tempDir = filesystem::temp_directory_path() / "payment_votes_test";
        filesystem::create_directories(tempDir);
        
        // Create test database
        testDbPath = tempDir / "test.db";
        int rc = sqlite3_open(testDbPath.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK);
        
        // Create Logger
        logger = make_unique<Logger>(Logger::LogLevel::DEBUG);
        
        // Create test data factory
        testDataFactory = make_unique<TestDataFactory>();
        
        // Create handler
        handler = make_unique<PaymentParticipantsVotesHandlerSQLite>(
            db, 
            "payment_votes_test", 
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
    PublicKey::Shared generateTestPublicKey() {
        byte keyData[kPublicKeySize];
        fill(keyData, keyData + kPublicKeySize, 0xAB);
        return make_shared<PublicKey>(keyData);
    }
    
    Signature::Shared generateTestSignature() {
        byte signatureData[kSignatureSize];
        fill(signatureData, signatureData + kSignatureSize, 0xCD);
        return make_shared<Signature>(signatureData);
    }
    
    TransactionUUID generateTestTransactionUUID() {
        return testDataFactory->generateTransactionUUID();
    }
    
    filesystem::path tempDir;
    filesystem::path testDbPath;
    sqlite3* db = nullptr;
    unique_ptr<Logger> logger;
    unique_ptr<TestDataFactory> testDataFactory;
    unique_ptr<PaymentParticipantsVotesHandlerSQLite> handler;
};

// Constructor Tests
TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, Constructor_ValidParameters_CreatesTableAndIndexes) {
    // Verify table exists
    string query = "SELECT name FROM sqlite_master WHERE type='table' AND name='payment_votes_test';";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt);
    EXPECT_EQ(rc, SQLITE_ROW);
    
    string tableName = (char*)sqlite3_column_text(stmt, 0);
    EXPECT_EQ(tableName, "payment_votes_test");
    
    sqlite3_finalize(stmt);
}

TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, Constructor_NullDatabase_ThrowsException) {
    EXPECT_THROW(
        PaymentParticipantsVotesHandlerSQLite(nullptr, "test_table", *logger),
        std::exception
    );
}

TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, Constructor_EmptyTableName_ThrowsException) {
    EXPECT_THROW(
        PaymentParticipantsVotesHandlerSQLite(db, "", *logger),
        std::exception
    );
}

// saveRecord Tests
TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, SaveRecord_ValidParameters_SavesSuccessfully) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    Contractor::Shared contractor = testDataFactory->generateContractor();
    PaymentNodeID paymentNodeID = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    Signature::Shared signature = generateTestSignature();
    
    EXPECT_NO_THROW(
        handler->saveRecord(transactionUUID, contractor, paymentNodeID, publicKey, signature)
    );
    
    // Verify record was saved
    map<PaymentNodeID, Signature::Shared> signatures = handler->participantsSignatures(transactionUUID);
    EXPECT_EQ(signatures.size(), 1);
    EXPECT_NE(signatures[paymentNodeID], nullptr);
}

TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, SaveRecord_NullContractor_ThrowsException) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    PaymentNodeID paymentNodeID = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    Signature::Shared signature = generateTestSignature();
    
    EXPECT_THROW(
        handler->saveRecord(transactionUUID, nullptr, paymentNodeID, publicKey, signature),
        ValueError
    );
}

TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, SaveRecord_NullPublicKey_ThrowsException) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    Contractor::Shared contractor = testDataFactory->generateContractor();
    PaymentNodeID paymentNodeID = 1;
    Signature::Shared signature = generateTestSignature();
    
    EXPECT_THROW(
        handler->saveRecord(transactionUUID, contractor, paymentNodeID, nullptr, signature),
        ValueError
    );
}

TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, SaveRecord_NullSignature_ThrowsException) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    Contractor::Shared contractor = testDataFactory->generateContractor();
    PaymentNodeID paymentNodeID = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    
    EXPECT_THROW(
        handler->saveRecord(transactionUUID, contractor, paymentNodeID, publicKey, nullptr),
        ValueError
    );
}

TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, SaveRecord_MultipleRecords_SavesSuccessfully) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    const int numRecords = 5;
    
    for (int i = 0; i < numRecords; ++i) {
        Contractor::Shared contractor = testDataFactory->generateContractor();
        PaymentNodeID paymentNodeID = i + 1;
        PublicKey::Shared publicKey = generateTestPublicKey();
        Signature::Shared signature = generateTestSignature();
        
        EXPECT_NO_THROW(
            handler->saveRecord(transactionUUID, contractor, paymentNodeID, publicKey, signature)
        );
    }
    
    // Verify all records were saved
    map<PaymentNodeID, Signature::Shared> signatures = handler->participantsSignatures(transactionUUID);
    EXPECT_EQ(signatures.size(), numRecords);
}

TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, SaveRecord_DuplicatePaymentNodeID_ThrowsException) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    Contractor::Shared contractor = testDataFactory->generateContractor();
    PaymentNodeID paymentNodeID = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    Signature::Shared signature1 = generateTestSignature();
    Signature::Shared signature2 = generateTestSignature();
    
    // Save first record
    EXPECT_NO_THROW(
        handler->saveRecord(transactionUUID, contractor, paymentNodeID, publicKey, signature1)
    );
    
    // Try to save duplicate
    EXPECT_THROW(
        handler->saveRecord(transactionUUID, contractor, paymentNodeID, publicKey, signature2),
        IOError
    );
}

// participantsSignatures Tests
TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, ParticipantsSignatures_WithRecords_ReturnsCorrectSignatures) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    const int numRecords = 3;
    
    map<PaymentNodeID, Signature::Shared> expectedSignatures;
    
    for (int i = 0; i < numRecords; ++i) {
        Contractor::Shared contractor = testDataFactory->generateContractor();
        PaymentNodeID paymentNodeID = i + 1;
        PublicKey::Shared publicKey = generateTestPublicKey();
        Signature::Shared signature = generateTestSignature();
        
        expectedSignatures[paymentNodeID] = signature;
        
        handler->saveRecord(transactionUUID, contractor, paymentNodeID, publicKey, signature);
    }
    
    map<PaymentNodeID, Signature::Shared> retrievedSignatures = handler->participantsSignatures(transactionUUID);
    
    EXPECT_EQ(retrievedSignatures.size(), expectedSignatures.size());
    for (const auto& pair : expectedSignatures) {
        EXPECT_NE(retrievedSignatures[pair.first], nullptr);
    }
}

TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, ParticipantsSignatures_NoRecords_ReturnsEmptyMap) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    
    map<PaymentNodeID, Signature::Shared> signatures = handler->participantsSignatures(transactionUUID);
    EXPECT_TRUE(signatures.empty());
}

TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, ParticipantsSignatures_DifferentTransactions_ReturnsCorrectSignatures) {
    TransactionUUID transactionUUID1 = generateTestTransactionUUID();
    TransactionUUID transactionUUID2 = generateTestTransactionUUID();
    
    // Save records for first transaction
    for (int i = 0; i < 3; ++i) {
        Contractor::Shared contractor = testDataFactory->generateContractor();
        PaymentNodeID paymentNodeID = i + 1;
        PublicKey::Shared publicKey = generateTestPublicKey();
        Signature::Shared signature = generateTestSignature();
        
        handler->saveRecord(transactionUUID1, contractor, paymentNodeID, publicKey, signature);
    }
    
    // Save records for second transaction
    for (int i = 0; i < 2; ++i) {
        Contractor::Shared contractor = testDataFactory->generateContractor();
        PaymentNodeID paymentNodeID = i + 1;
        PublicKey::Shared publicKey = generateTestPublicKey();
        Signature::Shared signature = generateTestSignature();
        
        handler->saveRecord(transactionUUID2, contractor, paymentNodeID, publicKey, signature);
    }
    
    // Verify signatures for first transaction
    map<PaymentNodeID, Signature::Shared> signatures1 = handler->participantsSignatures(transactionUUID1);
    EXPECT_EQ(signatures1.size(), 3);
    
    // Verify signatures for second transaction
    map<PaymentNodeID, Signature::Shared> signatures2 = handler->participantsSignatures(transactionUUID2);
    EXPECT_EQ(signatures2.size(), 2);
}

// deleteRecords Tests
TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, DeleteRecords_ExistingTransaction_DeletesSuccessfully) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    const int numRecords = 3;
    
    // Save multiple records
    for (int i = 0; i < numRecords; ++i) {
        Contractor::Shared contractor = testDataFactory->generateContractor();
        PaymentNodeID paymentNodeID = i + 1;
        PublicKey::Shared publicKey = generateTestPublicKey();
        Signature::Shared signature = generateTestSignature();
        
        handler->saveRecord(transactionUUID, contractor, paymentNodeID, publicKey, signature);
    }
    
    // Verify records exist
    map<PaymentNodeID, Signature::Shared> signaturesBefore = handler->participantsSignatures(transactionUUID);
    EXPECT_EQ(signaturesBefore.size(), numRecords);
    
    // Delete records
    EXPECT_NO_THROW(
        handler->deleteRecords(transactionUUID)
    );
    
    // Verify records are deleted
    map<PaymentNodeID, Signature::Shared> signaturesAfter = handler->participantsSignatures(transactionUUID);
    EXPECT_TRUE(signaturesAfter.empty());
}

TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, DeleteRecords_NonExistentTransaction_DoesNotThrow) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    
    EXPECT_NO_THROW(
        handler->deleteRecords(transactionUUID)
    );
}

TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, DeleteRecords_MultipleCalls_DoesNotThrow) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    Contractor::Shared contractor = testDataFactory->generateContractor();
    PaymentNodeID paymentNodeID = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    Signature::Shared signature = generateTestSignature();
    
    // Save record
    handler->saveRecord(transactionUUID, contractor, paymentNodeID, publicKey, signature);
    
    // Delete multiple times
    EXPECT_NO_THROW(handler->deleteRecords(transactionUUID));
    EXPECT_NO_THROW(handler->deleteRecords(transactionUUID));
    EXPECT_NO_THROW(handler->deleteRecords(transactionUUID));
}

// Integration Tests
TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, Integration_SaveRetrieveDelete_WorksCorrectly) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    Contractor::Shared contractor = testDataFactory->generateContractor();
    PaymentNodeID paymentNodeID = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    Signature::Shared signature = generateTestSignature();
    
    // Save record
    EXPECT_NO_THROW(
        handler->saveRecord(transactionUUID, contractor, paymentNodeID, publicKey, signature)
    );
    
    // Retrieve signatures
    map<PaymentNodeID, Signature::Shared> signatures = handler->participantsSignatures(transactionUUID);
    EXPECT_EQ(signatures.size(), 1);
    EXPECT_NE(signatures[paymentNodeID], nullptr);
    
    // Delete record
    EXPECT_NO_THROW(
        handler->deleteRecords(transactionUUID)
    );
    
    // Verify deletion
    signatures = handler->participantsSignatures(transactionUUID);
    EXPECT_TRUE(signatures.empty());
}

TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, Integration_MultipleTransactions_WorksCorrectly) {
    const int numTransactions = 3;
    const int numRecordsPerTransaction = 2;
    
    vector<TransactionUUID> transactionUUIDs;
    
    // Save records for multiple transactions
    for (int t = 0; t < numTransactions; ++t) {
        TransactionUUID transactionUUID = generateTestTransactionUUID();
        transactionUUIDs.push_back(transactionUUID);
        
        for (int r = 0; r < numRecordsPerTransaction; ++r) {
            Contractor::Shared contractor = testDataFactory->generateContractor();
            PaymentNodeID paymentNodeID = r + 1;
            PublicKey::Shared publicKey = generateTestPublicKey();
            Signature::Shared signature = generateTestSignature();
            
            handler->saveRecord(transactionUUID, contractor, paymentNodeID, publicKey, signature);
        }
    }
    
    // Verify all transactions have correct number of signatures
    for (const auto& uuid : transactionUUIDs) {
        map<PaymentNodeID, Signature::Shared> signatures = handler->participantsSignatures(uuid);
        EXPECT_EQ(signatures.size(), numRecordsPerTransaction);
    }
    
    // Delete one transaction
    handler->deleteRecords(transactionUUIDs[0]);
    
    // Verify first transaction is deleted
    map<PaymentNodeID, Signature::Shared> signatures = handler->participantsSignatures(transactionUUIDs[0]);
    EXPECT_TRUE(signatures.empty());
    
    // Verify other transactions still exist
    for (int i = 1; i < numTransactions; ++i) {
        signatures = handler->participantsSignatures(transactionUUIDs[i]);
        EXPECT_EQ(signatures.size(), numRecordsPerTransaction);
    }
}

// Performance Tests
TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, Performance_BulkOperations_CompletesInReasonableTime) {
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    const int numRecords = 100;
    
    auto start = chrono::high_resolution_clock::now();
    
    // Bulk save
    for (int i = 0; i < numRecords; ++i) {
        Contractor::Shared contractor = testDataFactory->generateContractor();
        PaymentNodeID paymentNodeID = i + 1;
        PublicKey::Shared publicKey = generateTestPublicKey();
        Signature::Shared signature = generateTestSignature();
        
        handler->saveRecord(transactionUUID, contractor, paymentNodeID, publicKey, signature);
    }
    
    // Bulk retrieve
    map<PaymentNodeID, Signature::Shared> signatures = handler->participantsSignatures(transactionUUID);
    EXPECT_EQ(signatures.size(), numRecords);
    
    // Delete all
    handler->deleteRecords(transactionUUID);
    
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time (5 seconds for 100 operations)
    EXPECT_LT(duration.count(), 5000);
}

// Error Handling Tests
TEST_F(PaymentParticipantsVotesHandlerSQLiteTest, ErrorHandling_CorruptedDatabase_ThrowsIOError) {
    // Close the database to simulate corruption
    sqlite3_close(db);
    db = nullptr;
    
    TransactionUUID transactionUUID = generateTestTransactionUUID();
    Contractor::Shared contractor = testDataFactory->generateContractor();
    PaymentNodeID paymentNodeID = 1;
    PublicKey::Shared publicKey = generateTestPublicKey();
    Signature::Shared signature = generateTestSignature();
    
    // Operations should throw IOError
    EXPECT_THROW(
        handler->saveRecord(transactionUUID, contractor, paymentNodeID, publicKey, signature),
        IOError
    );
    
    EXPECT_THROW(
        handler->participantsSignatures(transactionUUID),
        IOError
    );
} 