#include "gtest/gtest.h"
#include "../../../../src/core/io/storage/postgresql/FeaturesHandlerPostgreSQL.h"
#include "../../../../src/core/logger/Logger.h"
#include "../fixtures/DatabaseTestHelper.h"
#include "../fixtures/PostgreSQLTestFixtures.h"

class FeaturesHandlerPostgreSQLIntegrationTest : public ::testing::Test {
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
        
        if (!mConnection) {
            throw std::runtime_error("Failed to create database connection");
        }
        
        // Create handler instance
        mHandler = std::make_unique<FeaturesHandlerPostgreSQL>(mConnection, mTestTableName, mLogger);
        
        // Clean any existing test data
        cleanupTestData();
    }
    
    void TearDown() override {
        cleanupTestData();
        if (mConnection) {
            PQfinish(mConnection);
        }
    }
    
    void cleanupTestData() {
        if (mConnection) {
            std::string query = "DELETE FROM " + mTestTableName;
            PGresult* result = PQexec(mConnection, query.c_str());
            PQclear(result);
        }
    }
    
    // Helper methods for creating test data
    std::string createValidFeatureName() {
        return "test_feature_" + std::to_string(rand() % 10000);
    }
    
    std::string createDifferentFeatureName() {
        return "diff_feature_" + std::to_string(rand() % 10000);
    }
    
    std::string createValidFeatureValue() {
        return "test_value_" + std::to_string(rand() % 10000);
    }
    
    std::string createDifferentFeatureValue() {
        return "diff_value_" + std::to_string(rand() % 10000);
    }
    
    std::string createLongFeatureValue(size_t length = 1000) {
        std::string value;
        value.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            value += static_cast<char>('A' + (i % 26));
        }
        return value;
    }
    
    // Helper structure for raw database data validation
    struct RawFeatureData {
        std::string featureName;
        int featureLength;
        std::string featureValue;
    };
    
    // Raw database data validation method
    std::vector<RawFeatureData> getRawFeatureData() {
        std::vector<RawFeatureData> results;
        std::string query = "SELECT feature_name, feature_length, feature_value FROM " + mTestTableName + " ORDER BY feature_name";
        
        PGresult* result = PQexec(mConnection, query.c_str());
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to execute raw data query");
        }
        
        int rows = PQntuples(result);
        results.reserve(rows);
        
        for (int i = 0; i < rows; ++i) {
            RawFeatureData data;
            data.featureName = PQgetvalue(result, i, 0);
            data.featureLength = std::atoi(PQgetvalue(result, i, 1));
            data.featureValue = PQgetvalue(result, i, 2);
            results.push_back(data);
        }
        
        PQclear(result);
        return results;
    }
    
    RawFeatureData getRawFeatureData(const std::string& featureName) {
        std::string query = "SELECT feature_name, feature_length, feature_value FROM " + mTestTableName + 
                           " WHERE feature_name = '" + featureName + "'";
        
        PGresult* result = PQexec(mConnection, query.c_str());
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to execute raw data query for specific feature");
        }
        
        if (PQntuples(result) == 0) {
            PQclear(result);
            throw std::runtime_error("Feature not found in raw data");
        }
        
        RawFeatureData data;
        data.featureName = PQgetvalue(result, 0, 0);
        data.featureLength = std::atoi(PQgetvalue(result, 0, 1));
        data.featureValue = PQgetvalue(result, 0, 2);
        
        PQclear(result);
        return data;
    }
    
    int getFeatureCount() {
        std::string query = "SELECT COUNT(*) FROM " + mTestTableName;
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
            PQclear(result);
            return -1;
        }
        
        int count = std::atoi(PQgetvalue(result, 0, 0));
        PQclear(result);
        return count;
    }
    
    // Helper method to insert feature data directly via SQL
    void insertFeatureDirectly(const std::string& featureName, const std::string& featureValue) {
        std::string query = "INSERT INTO " + mTestTableName + 
                           " (feature_name, feature_length, feature_value) VALUES ('" + 
                           featureName + "', " + std::to_string(featureValue.length()) + ", '" + 
                           featureValue + "')";
        
        PGresult* result = PQexec(mConnection, query.c_str());
        
        if (PQresultStatus(result) != PGRES_COMMAND_OK) {
            PQclear(result);
            throw std::runtime_error("Failed to insert feature directly");
        }
        
        PQclear(result);
    }
    
    PGconn* mConnection;
    std::unique_ptr<FeaturesHandlerPostgreSQL> mHandler;
    Logger mLogger;
    std::string mTestTableName = "test_features";
};

// Test: saveFeature - valid feature saves successfully
TEST_F(FeaturesHandlerPostgreSQLIntegrationTest, saveFeature_ValidFeature_SavesSuccessfully) {
    // Arrange
    std::string featureName = createValidFeatureName();
    std::string featureValue = createValidFeatureValue();
    
    // Act
    ASSERT_NO_THROW(mHandler->saveFeature(featureName, featureValue));
    
    // Assert
    EXPECT_EQ(getFeatureCount(), 1);
    
    // Verify data can be retrieved
    std::string retrievedValue = mHandler->getFeature(featureName);
    EXPECT_EQ(retrievedValue, featureValue);
}

// Test: saveFeature - multiple features save successfully
TEST_F(FeaturesHandlerPostgreSQLIntegrationTest, saveFeature_MultipleFeatures_SavesAllSuccessfully) {
    // Arrange
    std::string featureName1 = createValidFeatureName();
    std::string featureName2 = createDifferentFeatureName();
    std::string featureValue1 = createValidFeatureValue();
    std::string featureValue2 = createDifferentFeatureValue();
    
    // Act
    ASSERT_NO_THROW(mHandler->saveFeature(featureName1, featureValue1));
    ASSERT_NO_THROW(mHandler->saveFeature(featureName2, featureValue2));
    
    // Assert
    EXPECT_EQ(getFeatureCount(), 2);
    
    // Verify both features can be retrieved
    EXPECT_EQ(mHandler->getFeature(featureName1), featureValue1);
    EXPECT_EQ(mHandler->getFeature(featureName2), featureValue2);
}

// Test: saveFeature - duplicate feature name updates existing record
TEST_F(FeaturesHandlerPostgreSQLIntegrationTest, saveFeature_DuplicateFeatureName_UpdatesExistingRecord) {
    // Arrange
    std::string featureName = createValidFeatureName();
    std::string originalValue = createValidFeatureValue();
    std::string updatedValue = createDifferentFeatureValue();
    
    // Act - Save original feature
    ASSERT_NO_THROW(mHandler->saveFeature(featureName, originalValue));
    EXPECT_EQ(getFeatureCount(), 1);
    
    // Act - Save updated feature with same name
    ASSERT_NO_THROW(mHandler->saveFeature(featureName, updatedValue));
    
    // Assert
    EXPECT_EQ(getFeatureCount(), 1); // Still only one record
    
    // Verify the record was updated with new value
    std::string retrievedValue = mHandler->getFeature(featureName);
    EXPECT_EQ(retrievedValue, updatedValue);
}

// Test: saveFeature - empty feature name throws ValueError
TEST_F(FeaturesHandlerPostgreSQLIntegrationTest, saveFeature_EmptyFeatureName_ThrowsValueError) {
    // Arrange
    std::string emptyFeatureName = "";
    std::string featureValue = createValidFeatureValue();
    
    // Act & Assert
    EXPECT_THROW(mHandler->saveFeature(emptyFeatureName, featureValue), ValueError);
}

// Test: saveFeature - empty feature value saves successfully
TEST_F(FeaturesHandlerPostgreSQLIntegrationTest, saveFeature_EmptyFeatureValue_SavesSuccessfully) {
    // Arrange
    std::string featureName = createValidFeatureName();
    std::string emptyValue = "";
    
    // Act
    ASSERT_NO_THROW(mHandler->saveFeature(featureName, emptyValue));
    
    // Assert
    EXPECT_EQ(getFeatureCount(), 1);
    
    // Verify empty value can be retrieved
    std::string retrievedValue = mHandler->getFeature(featureName);
    EXPECT_EQ(retrievedValue, emptyValue);
}

// Test: saveFeature - long feature value saves successfully
TEST_F(FeaturesHandlerPostgreSQLIntegrationTest, saveFeature_LongFeatureValue_SavesSuccessfully) {
    // Arrange
    std::string featureName = createValidFeatureName();
    std::string longValue = createLongFeatureValue(2000); // 2KB string
    
    // Act
    ASSERT_NO_THROW(mHandler->saveFeature(featureName, longValue));
    
    // Assert
    EXPECT_EQ(getFeatureCount(), 1);
    
    // Verify long value can be retrieved correctly
    std::string retrievedValue = mHandler->getFeature(featureName);
    EXPECT_EQ(retrievedValue, longValue);
}

// Test: getFeature - existing feature retrieved successfully
TEST_F(FeaturesHandlerPostgreSQLIntegrationTest, getFeature_ExistingFeature_RetrievesSuccessfully) {
    // Arrange
    std::string featureName = createValidFeatureName();
    std::string featureValue = createValidFeatureValue();
    
    mHandler->saveFeature(featureName, featureValue);
    
    // Act
    std::string retrievedValue = mHandler->getFeature(featureName);
    
    // Assert
    EXPECT_EQ(retrievedValue, featureValue);
}

// Test: getFeature - non-existent feature throws NotFoundError
TEST_F(FeaturesHandlerPostgreSQLIntegrationTest, getFeature_NonExistentFeature_ThrowsNotFoundError) {
    // Arrange
    std::string nonExistentFeatureName = "non_existent_feature_12345";
    
    // Act & Assert
    EXPECT_THROW(mHandler->getFeature(nonExistentFeatureName), NotFoundError);
}

// Test: getFeature - empty feature name throws ValueError
TEST_F(FeaturesHandlerPostgreSQLIntegrationTest, getFeature_EmptyFeatureName_ThrowsValueError) {
    // Arrange
    std::string emptyFeatureName = "";
    
    // Act & Assert
    EXPECT_THROW(mHandler->getFeature(emptyFeatureName), ValueError);
}

// Test: Raw Database Data Validation
TEST_F(FeaturesHandlerPostgreSQLIntegrationTest, saveFeature_ValidatesRawDatabaseData) {
    // Arrange
    std::string featureName = "test_feature_validation";
    std::string featureValue = "test_value_validation";
    
    // Act
    mHandler->saveFeature(featureName, featureValue);
    
    // Assert - Check raw database data
    RawFeatureData rawData = getRawFeatureData(featureName);
    EXPECT_EQ(rawData.featureName, featureName);
    EXPECT_EQ(rawData.featureLength, static_cast<int>(featureValue.length()));
    EXPECT_EQ(rawData.featureValue, featureValue);
}

// Test: Raw Database Data Validation - Multiple Features
TEST_F(FeaturesHandlerPostgreSQLIntegrationTest, saveFeature_MultipleFeatures_ValidatesRawDatabaseData) {
    // Arrange
    std::string featureName1 = "feature_1";
    std::string featureName2 = "feature_2";
    std::string featureValue1 = "value_1";
    std::string featureValue2 = "value_2";
    
    // Act
    mHandler->saveFeature(featureName1, featureValue1);
    mHandler->saveFeature(featureName2, featureValue2);
    
    // Assert - Check raw database data for multiple features
    std::vector<RawFeatureData> rawData = getRawFeatureData();
    EXPECT_EQ(rawData.size(), 2);
    
    // Data should be sorted by feature_name (ORDER BY in query)
    EXPECT_EQ(rawData[0].featureName, featureName1);
    EXPECT_EQ(rawData[0].featureLength, static_cast<int>(featureValue1.length()));
    EXPECT_EQ(rawData[0].featureValue, featureValue1);
    
    EXPECT_EQ(rawData[1].featureName, featureName2);
    EXPECT_EQ(rawData[1].featureLength, static_cast<int>(featureValue2.length()));
    EXPECT_EQ(rawData[1].featureValue, featureValue2);
}

// Test: Reverse Validation - INSERT through SQL → read through class
TEST_F(FeaturesHandlerPostgreSQLIntegrationTest, directInsert_ReadViaClassMethods_DeserializesCorrectly) {
    // Arrange
    std::string featureName = "direct_insert_feature";
    std::string featureValue = "direct_insert_value";
    
    // Act - Insert directly through SQL
    insertFeatureDirectly(featureName, featureValue);
    
    // Assert - Read through class methods
    std::string retrievedValue = mHandler->getFeature(featureName);
    EXPECT_EQ(retrievedValue, featureValue);
}

// Test: Reverse Validation - Multiple records
TEST_F(FeaturesHandlerPostgreSQLIntegrationTest, directInsert_MultipleFeatures_ReadViaClassMethods) {
    // Arrange
    std::string featureName1 = "direct_feature_1";
    std::string featureName2 = "direct_feature_2";
    std::string featureValue1 = "direct_value_1";
    std::string featureValue2 = "direct_value_2";
    
    // Act - Insert directly through SQL
    insertFeatureDirectly(featureName1, featureValue1);
    insertFeatureDirectly(featureName2, featureValue2);
    
    // Assert - Read through class methods
    EXPECT_EQ(mHandler->getFeature(featureName1), featureValue1);
    EXPECT_EQ(mHandler->getFeature(featureName2), featureValue2);
}

// Test: Cross-method validation - Save, Query, Update, Delete workflow
TEST_F(FeaturesHandlerPostgreSQLIntegrationTest, crossMethodValidation_SaveQueryUpdate_WorksCorrectly) {
    // Arrange
    std::string featureName = "cross_method_feature";
    std::string originalValue = "original_value";
    std::string updatedValue = "updated_value";
    
    // Act & Assert - Initial save
    ASSERT_NO_THROW(mHandler->saveFeature(featureName, originalValue));
    EXPECT_EQ(getFeatureCount(), 1);
    
    // Act & Assert - Query
    std::string retrievedValue = mHandler->getFeature(featureName);
    EXPECT_EQ(retrievedValue, originalValue);
    
    // Act & Assert - Update
    ASSERT_NO_THROW(mHandler->saveFeature(featureName, updatedValue));
    EXPECT_EQ(getFeatureCount(), 1); // Still one record
    
    // Act & Assert - Query updated value
    retrievedValue = mHandler->getFeature(featureName);
    EXPECT_EQ(retrievedValue, updatedValue);
    
    // Validate raw database data
    RawFeatureData rawData = getRawFeatureData(featureName);
    EXPECT_EQ(rawData.featureName, featureName);
    EXPECT_EQ(rawData.featureLength, static_cast<int>(updatedValue.length()));
    EXPECT_EQ(rawData.featureValue, updatedValue);
}

// Test: Table creation validates schema correctly
TEST_F(FeaturesHandlerPostgreSQLIntegrationTest, TableCreation_ValidatesSchemaCorrectly) {
    // Act - Query table schema
    std::string query = "SELECT column_name, data_type, is_nullable "
                       "FROM information_schema.columns "
                       "WHERE table_name = '" + mTestTableName + "' "
                       "ORDER BY ordinal_position";
    
    PGresult* result = PQexec(mConnection, query.c_str());
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    
    // Assert - Check table structure
    int rows = PQntuples(result);
    EXPECT_EQ(rows, 3); // Should have 3 columns
    
    if (rows >= 3) {
        // Check feature_name column
        EXPECT_STREQ(PQgetvalue(result, 0, 0), "feature_name");
        EXPECT_STREQ(PQgetvalue(result, 0, 1), "text");
        EXPECT_STREQ(PQgetvalue(result, 0, 2), "NO"); // NOT NULL
        
        // Check feature_length column
        EXPECT_STREQ(PQgetvalue(result, 1, 0), "feature_length");
        EXPECT_STREQ(PQgetvalue(result, 1, 1), "integer");
        EXPECT_STREQ(PQgetvalue(result, 1, 2), "NO"); // NOT NULL
        
        // Check feature_value column
        EXPECT_STREQ(PQgetvalue(result, 2, 0), "feature_value");
        EXPECT_STREQ(PQgetvalue(result, 2, 1), "text");
        EXPECT_STREQ(PQgetvalue(result, 2, 2), "NO"); // NOT NULL
    }
    
    PQclear(result);
    
    // Check primary key constraint
    query = "SELECT constraint_name FROM information_schema.table_constraints "
           "WHERE table_name = '" + mTestTableName + "' AND constraint_type = 'PRIMARY KEY'";
    
    result = PQexec(mConnection, query.c_str());
    ASSERT_EQ(PQresultStatus(result), PGRES_TUPLES_OK);
    EXPECT_EQ(PQntuples(result), 1); // Should have one primary key
    PQclear(result);
}

// Test: Constructor - null connection throws ValueError
TEST_F(FeaturesHandlerPostgreSQLIntegrationTest, Constructor_NullConnection_ThrowsValueError) {
    // Act & Assert
    EXPECT_THROW(
        FeaturesHandlerPostgreSQL(nullptr, "test_table", mLogger),
        ValueError
    );
}

// Test: Constructor - empty table name throws ValueError
TEST_F(FeaturesHandlerPostgreSQLIntegrationTest, Constructor_EmptyTableName_ThrowsValueError) {
    // Act & Assert
    EXPECT_THROW(
        FeaturesHandlerPostgreSQL(mConnection, "", mLogger),
        ValueError
    );
} 