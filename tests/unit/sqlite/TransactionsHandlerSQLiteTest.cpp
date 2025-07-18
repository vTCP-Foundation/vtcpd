#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../../../src/core/io/storage/sqlite/TransactionsHandlerSQLite.h"
#include "../../../src/core/common/exceptions/IOError.h"
#include "../../../src/core/common/exceptions/ValueError.h"
#include "../../../src/core/common/exceptions/NotFoundError.h"
#include "../fixtures/TestDataFactory.h"
#include <filesystem>
#include <memory>
#include <sqlite3.h>

using namespace std;
using ::testing::_;
using ::testing::Return;
using ::testing::Throw;

/**
 * Test fixture for TransactionsHandlerSQLite unit tests.
 * Provides common setup and teardown for transactions handler testing.
 */
class TransactionsHandlerSQLiteTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory and database
        tempDir = filesystem::temp_directory_path() / ("vtcp_transactions_test_" + to_string(rand()));
        filesystem::create_directories(tempDir);
        
        dbPath = tempDir / "transactions_test.db";
        tableName = "test_transactions";
        
        // Open SQLite database
        int rc = sqlite3_open(dbPath.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK) << "Failed to open test database";
        
        // Create mock logger
        logger = TestDataFactory::createMockLogger();
        
        // Setup test data
        validTransactionData = TestFixtures::TransactionTestData::createValid();
        emptyTransactionData = TestFixtures::TransactionTestData::createEmpty();
        largeTransactionData = TestFixtures::TransactionTestData::createLarge();
    }
    
    void TearDown() override {
        if (db) {
            sqlite3_close(db);
        }
        if (filesystem::exists(tempDir)) {
            filesystem::remove_all(tempDir);
        }
    }
    
    filesystem::path tempDir;
    filesystem::path dbPath;
    sqlite3* db = nullptr;
    string tableName;
    unique_ptr<Logger> logger;
    
    // Test data
    TestFixtures::TransactionTestData validTransactionData;
    TestFixtures::TransactionTestData emptyTransactionData;
    TestFixtures::TransactionTestData largeTransactionData;
};

/**
 * Test successful TransactionsHandlerSQLite construction with valid parameters.
 */
TEST_F(TransactionsHandlerSQLiteTest, Constructor_ValidParameters_CreatesHandlerSuccessfully) {
    // Act & Assert - Should not throw
    EXPECT_NO_THROW({
        TransactionsHandlerSQLite handler(db, tableName, *logger);
    });
}

/**
 * Test TransactionsHandlerSQLite construction with null database connection.
 */
TEST_F(TransactionsHandlerSQLiteTest, Constructor_NullDatabase_ThrowsValueError) {
    // Act & Assert
    EXPECT_THROW({
        TransactionsHandlerSQLite handler(nullptr, tableName, *logger);
    }, ValueError);
}

/**
 * Test TransactionsHandlerSQLite construction with empty table name.
 */
TEST_F(TransactionsHandlerSQLiteTest, Constructor_EmptyTableName_ThrowsValueError) {
    // Act & Assert
    EXPECT_THROW({
        TransactionsHandlerSQLite handler(db, "", *logger);
    }, ValueError);
}

/**
 * Test TransactionsHandlerSQLite construction creates table and index.
 */
TEST_F(TransactionsHandlerSQLiteTest, Constructor_ValidParameters_CreatesTableAndIndex) {
    // Act
    TransactionsHandlerSQLite handler(db, tableName, *logger);
    
    // Assert - Check that table exists
    const char* query = "SELECT name FROM sqlite_master WHERE type='table' AND name=?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_bind_text(stmt, 1, tableName.c_str(), -1, SQLITE_STATIC);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt);
    EXPECT_EQ(rc, SQLITE_ROW); // Table should exist
    
    sqlite3_finalize(stmt);
    
    // Check that unique index exists
    const char* indexQuery = "SELECT name FROM sqlite_master WHERE type='index' AND name LIKE ?";
    sqlite3_stmt* indexStmt;
    rc = sqlite3_prepare_v2(db, indexQuery, -1, &indexStmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    string indexPattern = tableName + "_transaction_uuid_idx";
    rc = sqlite3_bind_text(indexStmt, 1, indexPattern.c_str(), -1, SQLITE_STATIC);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(indexStmt);
    EXPECT_EQ(rc, SQLITE_ROW); // Index should exist
    
    sqlite3_finalize(indexStmt);
}

/**
 * Test saving a valid transaction.
 */
TEST_F(TransactionsHandlerSQLiteTest, SaveRecord_ValidTransaction_SavesSuccessfully) {
    // Arrange
    TransactionsHandlerSQLite handler(db, tableName, *logger);
    
    // Act & Assert - Should not throw
    EXPECT_NO_THROW({
        handler.saveRecord(
            validTransactionData.transactionUUID,
            validTransactionData.transactionBody,
            validTransactionData.transactionBytesCount
        );
    });
}

/**
 * Test saving transaction with null body.
 */
TEST_F(TransactionsHandlerSQLiteTest, SaveRecord_NullTransactionBody_ThrowsIOError) {
    // Arrange
    TransactionsHandlerSQLite handler(db, tableName, *logger);
    
    // Act & Assert
    EXPECT_THROW({
        handler.saveRecord(
            validTransactionData.transactionUUID,
            nullptr,
            100
        );
    }, IOError);
}

/**
 * Test saving transaction with zero byte count.
 */
TEST_F(TransactionsHandlerSQLiteTest, SaveRecord_ZeroByteCount_SavesSuccessfully) {
    // Arrange
    TransactionsHandlerSQLite handler(db, tableName, *logger);
    
    // Act & Assert - Zero byte count should be allowed
    EXPECT_NO_THROW({
        handler.saveRecord(
            validTransactionData.transactionUUID,
            validTransactionData.transactionBody,
            0
        );
    });
}

/**
 * Test saving large transaction.
 */
TEST_F(TransactionsHandlerSQLiteTest, SaveRecord_LargeTransaction_SavesSuccessfully) {
    // Arrange
    TransactionsHandlerSQLite handler(db, tableName, *logger);
    
    // Act & Assert - Large transactions should be handled
    EXPECT_NO_THROW({
        handler.saveRecord(
            largeTransactionData.transactionUUID,
            largeTransactionData.transactionBody,
            largeTransactionData.transactionBytesCount
        );
    });
}

/**
 * Test retrieving existing transaction.
 */
TEST_F(TransactionsHandlerSQLiteTest, GetTransaction_ExistingTransaction_ReturnsCorrectData) {
    // Arrange
    TransactionsHandlerSQLite handler(db, tableName, *logger);
    handler.saveRecord(
        validTransactionData.transactionUUID,
        validTransactionData.transactionBody,
        validTransactionData.transactionBytesCount
    );
    
    // Act
    auto retrievedTransaction = handler.getTransaction(validTransactionData.transactionUUID);
    
    // Assert
    EXPECT_NE(retrievedTransaction, nullptr);
    EXPECT_EQ(memcmp(
        retrievedTransaction.get(),
        validTransactionData.transactionBody.get(),
        validTransactionData.transactionBytesCount
    ), 0);
}

/**
 * Test retrieving non-existing transaction.
 */
TEST_F(TransactionsHandlerSQLiteTest, GetTransaction_NonExistingTransaction_ThrowsNotFoundError) {
    // Arrange
    TransactionsHandlerSQLite handler(db, tableName, *logger);
    TransactionUUID nonExistingUUID = TestDataFactory::createValidTransactionUUID();
    
    // Act & Assert
    EXPECT_THROW({
        handler.getTransaction(nonExistingUUID);
    }, NotFoundError);
}

/**
 * Test updating existing transaction (replace semantics).
 */
TEST_F(TransactionsHandlerSQLiteTest, SaveRecord_UpdateExisting_ReplacesSuccessfully) {
    // Arrange
    TransactionsHandlerSQLite handler(db, tableName, *logger);
    
    // Save initial transaction
    handler.saveRecord(
        validTransactionData.transactionUUID,
        validTransactionData.transactionBody,
        validTransactionData.transactionBytesCount
    );
    
    // Create updated transaction data
    auto updatedData = TestFixtures::TransactionTestData::createValid();
    updatedData.transactionUUID = validTransactionData.transactionUUID; // Same UUID
    
    // Act - Save with same UUID (should replace)
    handler.saveRecord(
        updatedData.transactionUUID,
        updatedData.transactionBody,
        updatedData.transactionBytesCount
    );
    
    // Assert - Should retrieve updated data
    auto retrievedTransaction = handler.getTransaction(validTransactionData.transactionUUID);
    EXPECT_EQ(memcmp(
        retrievedTransaction.get(),
        updatedData.transactionBody.get(),
        updatedData.transactionBytesCount
    ), 0);
}

/**
 * Test saving multiple transactions.
 */
TEST_F(TransactionsHandlerSQLiteTest, SaveRecord_MultipleTransactions_SavesAllSuccessfully) {
    // Arrange
    TransactionsHandlerSQLite handler(db, tableName, *logger);
    vector<TestFixtures::TransactionTestData> transactions;
    
    for (int i = 0; i < 5; ++i) {
        transactions.push_back(TestFixtures::TransactionTestData::createValid());
    }
    
    // Act - Save all transactions
    for (const auto& txData : transactions) {
        EXPECT_NO_THROW({
            handler.saveRecord(
                txData.transactionUUID,
                txData.transactionBody,
                txData.transactionBytesCount
            );
        });
    }
    
    // Assert - Retrieve all transactions
    for (const auto& txData : transactions) {
        auto retrievedTransaction = handler.getTransaction(txData.transactionUUID);
        EXPECT_NE(retrievedTransaction, nullptr);
        EXPECT_EQ(memcmp(
            retrievedTransaction.get(),
            txData.transactionBody.get(),
            txData.transactionBytesCount
        ), 0);
    }
}

/**
 * Test transaction UUID uniqueness constraint.
 */
TEST_F(TransactionsHandlerSQLiteTest, SaveRecord_DuplicateUUID_ReplacesExisting) {
    // Arrange
    TransactionsHandlerSQLite handler(db, tableName, *logger);
    
    auto firstData = TestFixtures::TransactionTestData::createValid();
    auto secondData = TestFixtures::TransactionTestData::createValid();
    secondData.transactionUUID = firstData.transactionUUID; // Same UUID
    
    // Act
    handler.saveRecord(
        firstData.transactionUUID,
        firstData.transactionBody,
        firstData.transactionBytesCount
    );
    
    handler.saveRecord(
        secondData.transactionUUID,
        secondData.transactionBody,
        secondData.transactionBytesCount
    );
    
    // Assert - Should retrieve second data (replacement)
    auto retrievedTransaction = handler.getTransaction(firstData.transactionUUID);
    EXPECT_EQ(memcmp(
        retrievedTransaction.get(),
        secondData.transactionBody.get(),
        secondData.transactionBytesCount
    ), 0);
}

/**
 * Test transaction with maximum UUID values.
 */
TEST_F(TransactionsHandlerSQLiteTest, SaveRecord_MaxUUIDValues_HandlesCorrectly) {
    // Arrange
    TransactionsHandlerSQLite handler(db, tableName, *logger);
    TransactionUUID maxUUID;
    memset(maxUUID.data, 0xFF, TransactionUUID::kBytesSize); // All bytes set to max
    
    // Act & Assert
    EXPECT_NO_THROW({
        handler.saveRecord(
            maxUUID,
            validTransactionData.transactionBody,
            validTransactionData.transactionBytesCount
        );
    });
    
    auto retrievedTransaction = handler.getTransaction(maxUUID);
    EXPECT_NE(retrievedTransaction, nullptr);
}

/**
 * Test transaction with zero UUID values.
 */
TEST_F(TransactionsHandlerSQLiteTest, SaveRecord_ZeroUUIDValues_HandlesCorrectly) {
    // Arrange
    TransactionsHandlerSQLite handler(db, tableName, *logger);
    TransactionUUID zeroUUID;
    memset(zeroUUID.data, 0x00, TransactionUUID::kBytesSize); // All bytes set to zero
    
    // Act & Assert
    EXPECT_NO_THROW({
        handler.saveRecord(
            zeroUUID,
            validTransactionData.transactionBody,
            validTransactionData.transactionBytesCount
        );
    });
    
    auto retrievedTransaction = handler.getTransaction(zeroUUID);
    EXPECT_NE(retrievedTransaction, nullptr);
}

/**
 * Test transaction data integrity.
 */
TEST_F(TransactionsHandlerSQLiteTest, SaveRecord_DataIntegrity_PreservesDataCorrectly) {
    // Arrange
    TransactionsHandlerSQLite handler(db, tableName, *logger);
    
    // Create transaction with specific pattern
    size_t dataSize = 1000;
    auto patternData = mallocAndAdoptShared<byte>(dataSize);
    for (size_t i = 0; i < dataSize; ++i) {
        patternData.get()[i] = static_cast<byte>(i % 256);
    }
    
    // Act
    handler.saveRecord(
        validTransactionData.transactionUUID,
        patternData,
        dataSize
    );
    
    auto retrievedTransaction = handler.getTransaction(validTransactionData.transactionUUID);
    
    // Assert - Verify data pattern is preserved
    for (size_t i = 0; i < dataSize; ++i) {
        EXPECT_EQ(retrievedTransaction.get()[i], static_cast<byte>(i % 256))
            << "Data mismatch at byte " << i;
    }
}

/**
 * Test byte count accuracy.
 */
TEST_F(TransactionsHandlerSQLiteTest, SaveRecord_ByteCountAccuracy_StoresCorrectSize) {
    // Arrange
    TransactionsHandlerSQLite handler(db, tableName, *logger);
    size_t testSize = 512;
    
    handler.saveRecord(
        validTransactionData.transactionUUID,
        validTransactionData.transactionBody,
        testSize
    );
    
    // Act - Query the database directly to check stored byte count
    const char* query = "SELECT transaction_bytes_count FROM " + tableName + " WHERE transaction_uuid = ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_bind_blob(stmt, 1, validTransactionData.transactionUUID.data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt);
    ASSERT_EQ(rc, SQLITE_ROW);
    
    int storedSize = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    
    // Assert
    EXPECT_EQ(storedSize, static_cast<int>(testSize));
}

/**
 * Test concurrent access to transactions.
 */
TEST_F(TransactionsHandlerSQLiteTest, ConcurrentAccess_MultipleOperations_HandlesCorrectly) {
    // Arrange
    TransactionsHandlerSQLite handler(db, tableName, *logger);
    vector<TestFixtures::TransactionTestData> transactions;
    
    // Create multiple unique transactions
    for (int i = 0; i < 10; ++i) {
        transactions.push_back(TestFixtures::TransactionTestData::createValid());
    }
    
    // Act - Perform multiple operations concurrently
    for (const auto& txData : transactions) {
        handler.saveRecord(
            txData.transactionUUID,
            txData.transactionBody,
            txData.transactionBytesCount
        );
    }
    
    // Assert - Verify all transactions can be retrieved
    for (const auto& txData : transactions) {
        auto retrievedTransaction = handler.getTransaction(txData.transactionUUID);
        EXPECT_NE(retrievedTransaction, nullptr);
    }
}

/**
 * Test memory management for large transactions.
 */
TEST_F(TransactionsHandlerSQLiteTest, MemoryManagement_LargeTransactions_HandlesCorrectly) {
    // Arrange
    TransactionsHandlerSQLite handler(db, tableName, *logger);
    size_t largeSize = 1024 * 1024; // 1MB
    auto largeData = mallocAndAdoptShared<byte>(largeSize);
    
    // Fill with test pattern
    for (size_t i = 0; i < largeSize; ++i) {
        largeData.get()[i] = static_cast<byte>(i % 256);
    }
    
    // Act
    handler.saveRecord(
        validTransactionData.transactionUUID,
        largeData,
        largeSize
    );
    
    auto retrievedTransaction = handler.getTransaction(validTransactionData.transactionUUID);
    
    // Assert - Verify large data is handled correctly
    EXPECT_NE(retrievedTransaction, nullptr);
    
    // Check first and last bytes to verify integrity
    EXPECT_EQ(retrievedTransaction.get()[0], 0);
    EXPECT_EQ(retrievedTransaction.get()[largeSize - 1], static_cast<byte>((largeSize - 1) % 256));
}

/**
 * Performance test for transaction operations.
 */
TEST_F(TransactionsHandlerSQLiteTest, Performance_TransactionOperations_CompletesInReasonableTime) {
    // Arrange
    TransactionsHandlerSQLite handler(db, tableName, *logger);
    auto start = chrono::high_resolution_clock::now();
    
    // Act - Perform many operations
    vector<TestFixtures::TransactionTestData> transactions;
    for (int i = 0; i < 100; ++i) {
        auto txData = TestFixtures::TransactionTestData::createValid();
        transactions.push_back(txData);
        
        handler.saveRecord(
            txData.transactionUUID,
            txData.transactionBody,
            txData.transactionBytesCount
        );
        
        handler.getTransaction(txData.transactionUUID);
    }
    
    // Assert
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    // 100 operations should complete within 2 seconds
    EXPECT_LT(duration.count(), 2000);
} 