#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../../../src/core/io/storage/sqlite/FeaturesHandlerSQLite.h"
#include "../../../src/core/common/exceptions/IOError.h"
#include "../../../src/core/common/exceptions/ValueError.h"
#include "../../../src/core/common/exceptions/NotFoundError.h"
#include <filesystem>
#include <memory>
#include <sqlite3.h>
#include <chrono>
#include "../../../src/core/logger/Logger.h"
#include "../../../src/core/common/Types.h"

using namespace std;
using ::testing::_;
using ::testing::Return;
using ::testing::Throw;

/**
 * Test fixture for FeaturesHandlerSQLite unit tests.
 * Provides common setup and teardown for features handler testing.
 */
class FeaturesHandlerSQLiteTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory and database
        tempDir = filesystem::temp_directory_path() / ("vtcp_features_test_" + to_string(rand()));
        filesystem::create_directories(tempDir);
        
        dbPath = tempDir / "features_test.db";
        tableName = "test_features";
        
        // Open SQLite database
        int rc = sqlite3_open(dbPath.c_str(), &db);
        ASSERT_EQ(rc, SQLITE_OK) << "Failed to open test database";
        
        // Create simple logger
        logger = make_unique<Logger>();
        
        // Setup test data (simple predefined values)
        validFeatureName = "feature_valid";
        validFeatureValue = "value_valid";
        emptyFeatureName = "";
        emptyFeatureValue = "";
        longFeatureName = string(256, 'N'); // long name 256 chars
        longFeatureValue = string(1024, 'V');
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
    string validFeatureName;
    string validFeatureValue;
    string emptyFeatureName;
    string emptyFeatureValue;
    string longFeatureName;
    string longFeatureValue;
};

/**
 * Test successful FeaturesHandlerSQLite construction with valid parameters.
 */
TEST_F(FeaturesHandlerSQLiteTest, Constructor_ValidParameters_CreatesHandlerSuccessfully) {
    // Act & Assert - Should not throw
    EXPECT_NO_THROW({
        FeaturesHandlerSQLite handler(db, tableName, *logger);
    });
}

/**
 * Test FeaturesHandlerSQLite construction with null database connection.
 */
TEST_F(FeaturesHandlerSQLiteTest, Constructor_NullDatabase_ThrowsValueError) {
    // Act & Assert
    EXPECT_THROW({
        FeaturesHandlerSQLite handler(nullptr, tableName, *logger);
    }, ValueError);
}

/**
 * Test FeaturesHandlerSQLite construction with empty table name.
 */
TEST_F(FeaturesHandlerSQLiteTest, Constructor_EmptyTableName_ThrowsValueError) {
    // Act & Assert
    EXPECT_THROW({
        FeaturesHandlerSQLite handler(db, "", *logger);
    }, ValueError);
}

/**
 * Test FeaturesHandlerSQLite construction creates table and index.
 */
TEST_F(FeaturesHandlerSQLiteTest, Constructor_ValidParameters_CreatesTableAndIndex) {
    // Act
    FeaturesHandlerSQLite handler(db, tableName, *logger);
    
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
    
    // Check that index exists
    const char* indexQuery = "SELECT name FROM sqlite_master WHERE type='index' AND name LIKE ?";
    sqlite3_stmt* indexStmt;
    rc = sqlite3_prepare_v2(db, indexQuery, -1, &indexStmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    string indexPattern = tableName + "_feature_name_idx";
    rc = sqlite3_bind_text(indexStmt, 1, indexPattern.c_str(), -1, SQLITE_STATIC);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(indexStmt);
    EXPECT_EQ(rc, SQLITE_ROW); // Index should exist
    
    sqlite3_finalize(indexStmt);
}

/**
 * Test saving a valid feature.
 */
TEST_F(FeaturesHandlerSQLiteTest, SaveFeature_ValidFeature_SavesSuccessfully) {
    // Arrange
    FeaturesHandlerSQLite handler(db, tableName, *logger);
    
    // Act & Assert - Should not throw
    EXPECT_NO_THROW({
        handler.saveFeature(validFeatureName, validFeatureValue);
    });
}

/**
 * Test saving feature with empty name.
 */
TEST_F(FeaturesHandlerSQLiteTest, SaveFeature_EmptyName_ThrowsValueError) {
    // Arrange
    FeaturesHandlerSQLite handler(db, tableName, *logger);
    
    // Act & Assert - Empty name should cause ValueError
    EXPECT_THROW({
        handler.saveFeature(emptyFeatureName, validFeatureValue);
    }, ValueError);
}

/**
 * Test saving feature with empty value.
 */
TEST_F(FeaturesHandlerSQLiteTest, SaveFeature_EmptyValue_SavesSuccessfully) {
    // Arrange
    FeaturesHandlerSQLite handler(db, tableName, *logger);
    
    // Act & Assert - Empty value should be allowed
    EXPECT_NO_THROW({
        handler.saveFeature(validFeatureName, emptyFeatureValue);
    });
}

/**
 * Test saving feature with long name and value.
 */
TEST_F(FeaturesHandlerSQLiteTest, SaveFeature_LongFeature_SavesSuccessfully) {
    // Arrange
    FeaturesHandlerSQLite handler(db, tableName, *logger);
    
    // Act & Assert - Long features should be handled
    EXPECT_NO_THROW({
        handler.saveFeature(longFeatureName, longFeatureValue);
    });
}

/**
 * Test retrieving existing feature.
 */
TEST_F(FeaturesHandlerSQLiteTest, GetFeature_ExistingFeature_ReturnsCorrectValue) {
    // Arrange
    FeaturesHandlerSQLite handler(db, tableName, *logger);
    handler.saveFeature(validFeatureName, validFeatureValue);
    
    // Act
    string retrievedValue = handler.getFeature(validFeatureName);
    
    // Assert
    EXPECT_EQ(retrievedValue, validFeatureValue);
}

/**
 * Test retrieving non-existing feature.
 */
TEST_F(FeaturesHandlerSQLiteTest, GetFeature_NonExistingFeature_ThrowsNotFoundError) {
    // Arrange
    FeaturesHandlerSQLite handler(db, tableName, *logger);
    string nonExistingFeature = "non_existing_feature_" + to_string(rand());
    
    // Act & Assert
    EXPECT_THROW({
        handler.getFeature(nonExistingFeature);
    }, NotFoundError);
}

/**
 * Test updating existing feature.
 */
TEST_F(FeaturesHandlerSQLiteTest, SaveFeature_UpdateExisting_UpdatesSuccessfully) {
    // Arrange
    FeaturesHandlerSQLite handler(db, tableName, *logger);
    string initialValue = "initial_value";
    string updatedValue = "updated_value";
    
    handler.saveFeature(validFeatureName, initialValue);
    
    // Act
    handler.saveFeature(validFeatureName, updatedValue);
    
    // Assert
    string retrievedValue = handler.getFeature(validFeatureName);
    EXPECT_EQ(retrievedValue, updatedValue);
}

/**
 * Test saving multiple features.
 */
TEST_F(FeaturesHandlerSQLiteTest, SaveFeature_MultipleFeatures_SavesAllSuccessfully) {
    // Arrange
    FeaturesHandlerSQLite handler(db, tableName, *logger);
    struct FeaturePair { string featureName; string featureValue; };
    vector<FeaturePair> features;
    for (int i = 0; i < 5; ++i) {
        features.push_back({"multi_feature_" + to_string(i), "multi_value_" + to_string(i)});
    }
    
    // Act - Save all features
    for (const auto& feature : features) {
        EXPECT_NO_THROW({
            handler.saveFeature(feature.featureName, feature.featureValue);
        });
    }
    
    // Assert - Retrieve all features
    for (const auto& feature : features) {
        string retrievedValue = handler.getFeature(feature.featureName);
        EXPECT_EQ(retrievedValue, feature.featureValue);
    }
}

/**
 * Test feature name uniqueness constraint.
 */
TEST_F(FeaturesHandlerSQLiteTest, SaveFeature_DuplicateName_ReplacesExisting) {
    // Arrange
    FeaturesHandlerSQLite handler(db, tableName, *logger);
    string firstValue = "first_value";
    string secondValue = "second_value";
    
    // Act
    handler.saveFeature(validFeatureName, firstValue);
    handler.saveFeature(validFeatureName, secondValue); // Should replace
    
    // Assert
    string retrievedValue = handler.getFeature(validFeatureName);
    EXPECT_EQ(retrievedValue, secondValue);
}

/**
 * Test retrieving feature with empty name.
 */
TEST_F(FeaturesHandlerSQLiteTest, GetFeature_EmptyName_ThrowsValueError) {
    // Arrange
    FeaturesHandlerSQLite handler(db, tableName, *logger);
    
    // Act & Assert
    EXPECT_THROW({
        handler.getFeature(emptyFeatureName);
    }, ValueError);
}

/**
 * Test feature with special characters.
 */
TEST_F(FeaturesHandlerSQLiteTest, SaveFeature_SpecialCharacters_HandlesCorrectly) {
    // Arrange
    FeaturesHandlerSQLite handler(db, tableName, *logger);
    string specialName = "feature'with\"special&chars<>[]{}";
    string specialValue = "value'with\"special&chars<>[]{}";
    
    // Act & Assert
    EXPECT_NO_THROW({
        handler.saveFeature(specialName, specialValue);
    });
    
    string retrievedValue = handler.getFeature(specialName);
    EXPECT_EQ(retrievedValue, specialValue);
}

/**
 * Test feature with unicode characters.
 */
TEST_F(FeaturesHandlerSQLiteTest, SaveFeature_UnicodeCharacters_HandlesCorrectly) {
    // Arrange
    FeaturesHandlerSQLite handler(db, tableName, *logger);
    string unicodeName = "функція_测试_🚀";
    string unicodeValue = "значення_值_📊";
    
    // Act & Assert
    EXPECT_NO_THROW({
        handler.saveFeature(unicodeName, unicodeValue);
    });
    
    string retrievedValue = handler.getFeature(unicodeName);
    EXPECT_EQ(retrievedValue, unicodeValue);
}

/**
 * Test concurrent access to features.
 */
TEST_F(FeaturesHandlerSQLiteTest, ConcurrentAccess_MultipleOperations_HandlesCorrectly) {
    // Arrange
    FeaturesHandlerSQLite handler(db, tableName, *logger);
    
    // Act - Perform multiple operations
    for (int i = 0; i < 10; ++i) {
        string featureName = "feature_" + to_string(i);
        string featureValue = "value_" + to_string(i);
        
        EXPECT_NO_THROW({
            handler.saveFeature(featureName, featureValue);
        });
    }
    
    // Assert - Verify all features
    for (int i = 0; i < 10; ++i) {
        string featureName = "feature_" + to_string(i);
        string expectedValue = "value_" + to_string(i);
        
        string retrievedValue = handler.getFeature(featureName);
        EXPECT_EQ(retrievedValue, expectedValue);
    }
}

/**
 * Test large feature value handling.
 */
TEST_F(FeaturesHandlerSQLiteTest, SaveFeature_LargeValue_HandlesCorrectly) {
    // Arrange
    FeaturesHandlerSQLite handler(db, tableName, *logger);
    string largeValue(100000, 'A'); // 100KB of 'A' characters
    
    // Act & Assert
    EXPECT_NO_THROW({
        handler.saveFeature(validFeatureName, largeValue);
    });
    
    string retrievedValue = handler.getFeature(validFeatureName);
    EXPECT_EQ(retrievedValue, largeValue);
}

/**
 * Test feature length calculation.
 */
TEST_F(FeaturesHandlerSQLiteTest, SaveFeature_FeatureLength_CalculatedCorrectly) {
    // Arrange
    FeaturesHandlerSQLite handler(db, tableName, *logger);
    string testValue = "test_value_123";
    handler.saveFeature(validFeatureName, testValue);
    
    // Act - Query the database directly to check length
    string query = "SELECT feature_length FROM " + tableName + " WHERE feature_name = ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_bind_text(stmt, 1, validFeatureName.c_str(), -1, SQLITE_STATIC);
    ASSERT_EQ(rc, SQLITE_OK);
    
    rc = sqlite3_step(stmt);
    ASSERT_EQ(rc, SQLITE_ROW);
    
    int storedLength = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    
    // Assert
    EXPECT_EQ(storedLength, static_cast<int>(testValue.length()));
}

/**
 * Performance test for feature operations.
 */
TEST_F(FeaturesHandlerSQLiteTest, Performance_FeatureOperations_CompletesInReasonableTime) {
    // Arrange
    FeaturesHandlerSQLite handler(db, tableName, *logger);
    auto start = chrono::high_resolution_clock::now();
    
    // Act - Perform many operations
    for (int i = 0; i < 1000; ++i) {
        string featureName = "perf_feature_" + to_string(i);
        string featureValue = "perf_value_" + to_string(i);
        
        handler.saveFeature(featureName, featureValue);
        handler.getFeature(featureName);
    }
    
    // Assert
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
    
    // 1000 operations should complete within 20 seconds
    EXPECT_LT(duration.count(), 20000);
} 