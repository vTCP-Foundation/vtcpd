#include <gtest/gtest.h>
#include "../../src/core/settings/Settings.h"
#include "../../src/core/settings/DatabaseProviderType.h"
#include "../../src/core/io/storage/StorageProviderFactory.h"
#include "../../src/core/logger/Logger.h"
#include "../../src/libs/json/json.h"
#include <string>
#include <memory>

using namespace std;
using json = nlohmann::json;

class DatabaseConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        settings = make_unique<Settings>();
        logger = make_unique<Logger>();
    }
    
    void TearDown() override {
        settings.reset();
        logger.reset();
    }
    
    unique_ptr<Settings> settings;
    unique_ptr<Logger> logger;
};

// Test PostgreSQL URI parsing with database name
TEST_F(DatabaseConfigTest, PostgreSQLURIWithDatabase) {
    json testConfig = json::parse(R"({
        "database_config": "postgresql://user:pass@localhost:5432/mydb"
    })");
    
    DatabaseConfiguration config = settings->databaseConfiguration(&testConfig);
    
    EXPECT_EQ(config.providerType, DatabaseProviderType::PostgreSQL);
    EXPECT_EQ(config.host, "localhost");
    EXPECT_EQ(config.port, 5432);
    EXPECT_EQ(config.username, "user");
    EXPECT_EQ(config.password, "pass");
    EXPECT_EQ(config.database, "mydb");
}

// Test PostgreSQL URI parsing without database name
TEST_F(DatabaseConfigTest, PostgreSQLURIWithoutDatabase) {
    json testConfig = json::parse(R"({
        "database_config": "postgresql://user:pass@localhost:5432/"
    })");
    
    DatabaseConfiguration config = settings->databaseConfiguration(&testConfig);
    
    EXPECT_EQ(config.providerType, DatabaseProviderType::PostgreSQL);
    EXPECT_EQ(config.host, "localhost");
    EXPECT_EQ(config.port, 5432);
    EXPECT_EQ(config.username, "user");
    EXPECT_EQ(config.password, "pass");
    EXPECT_EQ(config.database, "");
}

// Test PostgreSQL URI parsing without slash (backward compatibility)
TEST_F(DatabaseConfigTest, PostgreSQLURIBackwardCompatibility) {
    json testConfig = json::parse(R"({
        "database_config": "postgresql://user:pass@localhost:5432"
    })");
    
    DatabaseConfiguration config = settings->databaseConfiguration(&testConfig);
    
    EXPECT_EQ(config.providerType, DatabaseProviderType::PostgreSQL);
    EXPECT_EQ(config.host, "localhost");
    EXPECT_EQ(config.port, 5432);
    EXPECT_EQ(config.username, "user");
    EXPECT_EQ(config.password, "pass");
    EXPECT_EQ(config.database, "");
}

// Test SQLite URI parsing (should remain unchanged)
TEST_F(DatabaseConfigTest, SQLiteURI) {
    json testConfig = json::parse(R"({
        "database_config": "sqlite3:///path/to/db"
    })");
    
    DatabaseConfiguration config = settings->databaseConfiguration(&testConfig);
    
    EXPECT_EQ(config.providerType, DatabaseProviderType::SQLite);
    EXPECT_EQ(config.directory, "/path/to/db");
}

// Test StorageProviderFactory connection string creation with database name
TEST_F(DatabaseConfigTest, PostgreSQLConnectionStringWithDatabase) {
    DatabaseConfiguration config("localhost", 5432, "user", "pass", "mydb");
    
    string connectionString = StorageProviderFactory::createPostgreSQLConnectionStringForTesting(config, "defaultdb");
    
    // Should use database name from config, not the provided default
    EXPECT_NE(connectionString.find("dbname=mydb"), string::npos);
    EXPECT_EQ(connectionString.find("dbname=defaultdb"), string::npos);
}

// Test StorageProviderFactory connection string creation without database name
TEST_F(DatabaseConfigTest, PostgreSQLConnectionStringWithoutDatabase) {
    DatabaseConfiguration config("localhost", 5432, "user", "pass");
    
    string connectionString = StorageProviderFactory::createPostgreSQLConnectionStringForTesting(config, "defaultdb");
    
    // Should use provided default database name
    EXPECT_NE(connectionString.find("dbname=defaultdb"), string::npos);
}

// Test DatabaseConfiguration constructors
TEST_F(DatabaseConfigTest, DatabaseConfigurationConstructors) {
    // Test SQLite constructor
    DatabaseConfiguration sqliteConfig("test_dir");
    EXPECT_EQ(sqliteConfig.providerType, DatabaseProviderType::SQLite);
    EXPECT_EQ(sqliteConfig.directory, "test_dir");
    
    // Test PostgreSQL constructor without database
    DatabaseConfiguration pgConfig("localhost", 5432, "user", "pass");
    EXPECT_EQ(pgConfig.providerType, DatabaseProviderType::PostgreSQL);
    EXPECT_EQ(pgConfig.host, "localhost");
    EXPECT_EQ(pgConfig.port, 5432);
    EXPECT_EQ(pgConfig.username, "user");
    EXPECT_EQ(pgConfig.password, "pass");
    EXPECT_EQ(pgConfig.database, "");
    
    // Test PostgreSQL constructor with database
    DatabaseConfiguration pgConfigWithDb("localhost", 5432, "user", "pass", "mydb");
    EXPECT_EQ(pgConfigWithDb.providerType, DatabaseProviderType::PostgreSQL);
    EXPECT_EQ(pgConfigWithDb.host, "localhost");
    EXPECT_EQ(pgConfigWithDb.port, 5432);
    EXPECT_EQ(pgConfigWithDb.username, "user");
    EXPECT_EQ(pgConfigWithDb.password, "pass");
    EXPECT_EQ(pgConfigWithDb.database, "mydb");
} 