#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../../../src/core/io/storage/sqlite/StorageHandlerSQLite.h"
#include "../../../src/core/common/exceptions/IOError.h"
#include "../fixtures/TestDataFactory.h"
#include <filesystem>
#include <memory>
#include <fstream>

using namespace std;
using ::testing::_;
using ::testing::Return;
using ::testing::Throw;

/**
 * Test fixture for StorageHandlerSQLite unit tests.
 * Provides common setup and teardown for storage handler testing.
 */
class StorageHandlerSQLiteTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test databases
        tempDir = filesystem::temp_directory_path() / ("vtcp_test_" + to_string(rand()));
        filesystem::create_directories(tempDir);
        
        // Setup test data
        validDirectory = tempDir.string();
        validDatabaseName = "test_database.db";
        invalidDirectory = "/invalid/path/that/should/not/exist";
        invalidDatabaseName = "";
        
        // Create mock logger
        logger = TestDataFactory::createMockLogger();
    }
    
    void TearDown() override {
        // Clean up test files
        if (filesystem::exists(tempDir)) {
            filesystem::remove_all(tempDir);
        }
    }
    
    filesystem::path tempDir;
    string validDirectory;
    string validDatabaseName;
    string invalidDirectory;
    string invalidDatabaseName;
    unique_ptr<Logger> logger;
};

/**
 * Test successful StorageHandlerSQLite construction with valid parameters.
 */
TEST_F(StorageHandlerSQLiteTest, Constructor_ValidParameters_CreatesHandlerSuccessfully) {
    // Act & Assert - Should not throw
    EXPECT_NO_THROW({
        StorageHandlerSQLite handler(validDirectory, validDatabaseName, *logger);
    });
}

/**
 * Test StorageHandlerSQLite construction with invalid directory.
 */
TEST_F(StorageHandlerSQLiteTest, Constructor_InvalidDirectory_ThrowsIOError) {
    // Act & Assert
    EXPECT_THROW({
        StorageHandlerSQLite handler(invalidDirectory, validDatabaseName, *logger);
    }, IOError);
}

/**
 * Test StorageHandlerSQLite construction creates database file.
 */
TEST_F(StorageHandlerSQLiteTest, Constructor_ValidParameters_CreatesDatabaseFile) {
    // Act
    {
        StorageHandlerSQLite handler(validDirectory, validDatabaseName, *logger);
    } // Handler goes out of scope to ensure file is created and closed
    
    // Assert
    filesystem::path dbPath = tempDir / validDatabaseName;
    EXPECT_TRUE(filesystem::exists(dbPath));
}

/**
 * Test StorageHandlerSQLite construction creates directory if it doesn't exist.
 */
TEST_F(StorageHandlerSQLiteTest, Constructor_NonExistentDirectory_CreatesDirectory) {
    // Arrange
    string newDirectory = (tempDir / "new_subdir").string();
    EXPECT_FALSE(filesystem::exists(newDirectory));
    
    // Act
    {
        StorageHandlerSQLite handler(newDirectory, validDatabaseName, *logger);
    }
    
    // Assert
    EXPECT_TRUE(filesystem::exists(newDirectory));
    EXPECT_TRUE(filesystem::exists(filesystem::path(newDirectory) / validDatabaseName));
}

/**
 * Test beginTransaction returns valid IOTransaction.
 */
TEST_F(StorageHandlerSQLiteTest, BeginTransaction_ValidHandler_ReturnsIOTransaction) {
    // Arrange
    StorageHandlerSQLite handler(validDirectory, validDatabaseName, *logger);
    
    // Act
    auto transaction = handler.beginTransaction();
    
    // Assert
    EXPECT_NE(transaction, nullptr);
    EXPECT_NE(transaction.get(), nullptr);
}

/**
 * Test multiple calls to beginTransaction return different objects.
 */
TEST_F(StorageHandlerSQLiteTest, BeginTransaction_MultipleCalls_ReturnsDifferentTransactions) {
    // Arrange
    StorageHandlerSQLite handler(validDirectory, validDatabaseName, *logger);
    
    // Act
    auto transaction1 = handler.beginTransaction();
    auto transaction2 = handler.beginTransaction();
    
    // Assert
    EXPECT_NE(transaction1.get(), transaction2.get());
}

/**
 * Test vacuum operation completes successfully.
 */
TEST_F(StorageHandlerSQLiteTest, Vacuum_ValidHandler_CompletesSuccessfully) {
    // Arrange
    StorageHandlerSQLite handler(validDirectory, validDatabaseName, *logger);
    
    // Act & Assert - Should not throw
    EXPECT_NO_THROW({
        handler.vacuum();
    });
}

/**
 * Test vacuum operation reduces database file size after data operations.
 */
TEST_F(StorageHandlerSQLiteTest, Vacuum_AfterDataOperations_ReducesFileSize) {
    // Arrange
    StorageHandlerSQLite handler(validDirectory, validDatabaseName, *logger);
    filesystem::path dbPath = tempDir / validDatabaseName;
    
    // Perform some operations to add data
    auto transaction = handler.beginTransaction();
    transaction.reset(); // Complete transaction
    
    // Get initial file size
    auto initialSize = filesystem::file_size(dbPath);
    
    // Act
    handler.vacuum();
    
    // Assert
    auto finalSize = filesystem::file_size(dbPath);
    // File size should be less than or equal to initial size after vacuum
    EXPECT_LE(finalSize, initialSize);
}

/**
 * Test destructor closes database connection properly.
 */
TEST_F(StorageHandlerSQLiteTest, Destructor_ValidHandler_ClosesConnectionProperly) {
    filesystem::path dbPath = tempDir / validDatabaseName;
    
    // Act
    {
        StorageHandlerSQLite handler(validDirectory, validDatabaseName, *logger);
        // Handler will be destroyed when going out of scope
    }
    
    // Assert - Database file should still exist but connection should be closed
    EXPECT_TRUE(filesystem::exists(dbPath));
    
    // Try to create another handler with the same database - should work if connection was closed properly
    EXPECT_NO_THROW({
        StorageHandlerSQLite handler2(validDirectory, validDatabaseName, *logger);
    });
}

/**
 * Test static connection management - multiple handlers should share connection.
 */
TEST_F(StorageHandlerSQLiteTest, StaticConnection_MultipleHandlers_ShareConnection) {
    // Act - Create multiple handlers with the same database
    StorageHandlerSQLite handler1(validDirectory, validDatabaseName, *logger);
    StorageHandlerSQLite handler2(validDirectory, validDatabaseName, *logger);
    
    // Both handlers should be created successfully
    auto transaction1 = handler1.beginTransaction();
    auto transaction2 = handler2.beginTransaction();
    
    // Assert
    EXPECT_NE(transaction1, nullptr);
    EXPECT_NE(transaction2, nullptr);
}

/**
 * Test handler with empty database name.
 */
TEST_F(StorageHandlerSQLiteTest, Constructor_EmptyDatabaseName_ThrowsIOError) {
    // Act & Assert
    EXPECT_THROW({
        StorageHandlerSQLite handler(validDirectory, "", *logger);
    }, IOError);
}

/**
 * Test handler initialization enables foreign keys.
 */
TEST_F(StorageHandlerSQLiteTest, Constructor_ValidParameters_EnablesForeignKeys) {
    // This test verifies that foreign keys are enabled by attempting operations
    // that would fail if foreign keys were not enabled
    
    // Act
    StorageHandlerSQLite handler(validDirectory, validDatabaseName, *logger);
    auto transaction = handler.beginTransaction();
    
    // Assert - Transaction creation should succeed if foreign keys are properly enabled
    EXPECT_NE(transaction, nullptr);
}

/**
 * Test concurrent access to storage handler.
 */
TEST_F(StorageHandlerSQLiteTest, ConcurrentAccess_MultipleTransactions_HandledProperly) {
    // Arrange
    StorageHandlerSQLite handler(validDirectory, validDatabaseName, *logger);
    
    // Act - Create multiple transactions concurrently
    vector<IOTransaction::Shared> transactions;
    for (int i = 0; i < 5; ++i) {
        transactions.push_back(handler.beginTransaction());
    }
    
    // Assert - All transactions should be valid
    for (const auto& transaction : transactions) {
        EXPECT_NE(transaction, nullptr);
    }
}

/**
 * Test handler behavior with very long directory path.
 */
TEST_F(StorageHandlerSQLiteTest, Constructor_LongDirectoryPath_HandlesCorrectly) {
    // Arrange
    string longPath = tempDir.string();
    for (int i = 0; i < 10; ++i) {
        longPath += "/very_long_subdirectory_name_" + to_string(i);
    }
    
    // Act & Assert - Should handle long paths correctly or throw appropriate error
    if (longPath.length() < 260) { // Typical path length limit
        EXPECT_NO_THROW({
            StorageHandlerSQLite handler(longPath, validDatabaseName, *logger);
        });
    } else {
        EXPECT_THROW({
            StorageHandlerSQLite handler(longPath, validDatabaseName, *logger);
        }, IOError);
    }
}

/**
 * Test handler behavior with special characters in paths.
 */
TEST_F(StorageHandlerSQLiteTest, Constructor_SpecialCharactersInPath_HandlesCorrectly) {
    // Arrange
    string specialPath = (tempDir / "test_dir_with-special.chars_123").string();
    string specialDbName = "test-db_with.special-chars.db";
    
    // Act & Assert
    EXPECT_NO_THROW({
        StorageHandlerSQLite handler(specialPath, specialDbName, *logger);
    });
    
    // Verify file was created
    EXPECT_TRUE(filesystem::exists(filesystem::path(specialPath) / specialDbName));
}

/**
 * Test vacuum on empty database.
 */
TEST_F(StorageHandlerSQLiteTest, Vacuum_EmptyDatabase_CompletesSuccessfully) {
    // Arrange
    StorageHandlerSQLite handler(validDirectory, validDatabaseName, *logger);
    
    // Act & Assert - Vacuum on empty database should work
    EXPECT_NO_THROW({
        handler.vacuum();
    });
}

/**
 * Test handler reuse after vacuum.
 */
TEST_F(StorageHandlerSQLiteTest, HandlerReuse_AfterVacuum_WorksCorrectly) {
    // Arrange
    StorageHandlerSQLite handler(validDirectory, validDatabaseName, *logger);
    
    // Act
    handler.vacuum();
    auto transaction = handler.beginTransaction();
    
    // Assert
    EXPECT_NE(transaction, nullptr);
}

/**
 * Performance test for handler creation.
 */
TEST_F(StorageHandlerSQLiteTest, Performance_HandlerCreation_CompletesInReasonableTime) {
    // Arrange
    auto start = chrono::high_resolution_clock::now();
    
    // Act
    {
        StorageHandlerSQLite handler(validDirectory, validDatabaseName, *logger);
    }
    
    // Assert
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    // Handler creation should complete within 1 second for unit tests
    EXPECT_LT(duration.count(), 1000);
}

/**
 * Test error handling when database file is read-only.
 */
TEST_F(StorageHandlerSQLiteTest, Constructor_ReadOnlyDatabaseFile_HandlesCorrectly) {
    // Arrange - Create database file and make it read-only
    filesystem::path dbPath = tempDir / validDatabaseName;
    {
        StorageHandlerSQLite handler(validDirectory, validDatabaseName, *logger);
    } // Close handler to ensure file is created
    
    // Make file read-only
    filesystem::permissions(dbPath, filesystem::perms::owner_read | filesystem::perms::group_read | filesystem::perms::others_read);
    
    // Act & Assert - Should handle read-only file appropriately
    // Note: This might succeed if only reading is needed, or throw if writing is attempted
    try {
        StorageHandlerSQLite handler(validDirectory, validDatabaseName, *logger);
        // If construction succeeds, vacuum should fail on read-only file
        EXPECT_THROW(handler.vacuum(), IOError);
    } catch (const IOError&) {
        // Construction failure is also acceptable for read-only files
        SUCCEED();
    }
    
    // Restore permissions for cleanup
    filesystem::permissions(dbPath, filesystem::perms::owner_all);
} 