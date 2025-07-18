#include "DatabaseTestHelper.h"
#include <stdexcept>
#include <iostream>
#include <sstream>

// Hardcoded database credentials for integration tests
const std::string DatabaseTestHelper::TEST_HOST = "127.0.0.1";
const int DatabaseTestHelper::TEST_PORT = 5432;
const std::string DatabaseTestHelper::TEST_USER = "vtcp_user";
const std::string DatabaseTestHelper::TEST_PASSWORD = "vtcp_pass";
const std::string DatabaseTestHelper::TEST_DB_NAME = "vtcpd_test";

PGconn* DatabaseTestHelper::createConnection(
    const std::string &host,
    int port,
    const std::string &user,
    const std::string &password,
    const std::string &dbName)
{
    std::stringstream connectionString;
    connectionString << "host=" << host
                     << " port=" << port
                     << " user=" << user
                     << " password=" << password
                     << " dbname=" << dbName;
    
    PGconn* connection = PQconnectdb(connectionString.str().c_str());
    
    if (PQstatus(connection) != CONNECTION_OK) {
        std::string error = PQerrorMessage(connection);
        PQfinish(connection);
        throw std::runtime_error("DatabaseTestHelper: Failed to connect to database: " + error);
    }
    
    return connection;
}

void DatabaseTestHelper::closeConnection(PGconn* connection)
{
    if (connection != nullptr) {
        PQfinish(connection);
    }
}

void DatabaseTestHelper::cleanupTable(PGconn* connection, const std::string &tableName)
{
    if (connection == nullptr) {
        throw std::runtime_error("DatabaseTestHelper: Database connection is null");
    }
    
    std::string query = "DELETE FROM " + tableName;
    executeQuery(connection, query);
}

void DatabaseTestHelper::cleanupAllTables(PGconn* connection)
{
    if (connection == nullptr) {
        throw std::runtime_error("DatabaseTestHelper: Database connection is null");
    }
    
    std::vector<std::string> tableNames = getTableNames(connection);
    
    for (const auto& tableName : tableNames) {
        try {
            cleanupTable(connection, tableName);
        } catch (const std::exception& e) {
            // Continue cleaning other tables even if one fails
            std::cerr << "Warning: Failed to clean table " << tableName << ": " << e.what() << std::endl;
        }
    }
}

void DatabaseTestHelper::executeQuery(PGconn* connection, const std::string &query)
{
    if (connection == nullptr) {
        throw std::runtime_error("DatabaseTestHelper: Database connection is null");
    }
    
    PGresult* result = PQexec(connection, query.c_str());
    
    if (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_TUPLES_OK) {
        std::string error = PQerrorMessage(connection);
        PQclear(result);
        throw std::runtime_error("DatabaseTestHelper: Query execution failed: " + error + " (Query: " + query + ")");
    }
    
    PQclear(result);
}

bool DatabaseTestHelper::tableExists(PGconn* connection, const std::string &tableName)
{
    if (connection == nullptr) {
        throw std::runtime_error("DatabaseTestHelper: Database connection is null");
    }
    
    std::string query = "SELECT EXISTS (SELECT 1 FROM information_schema.tables WHERE table_name = '" + tableName + "')";
    
    PGresult* result = PQexec(connection, query.c_str());
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        std::string error = PQerrorMessage(connection);
        PQclear(result);
        throw std::runtime_error("DatabaseTestHelper: Table existence check failed: " + error);
    }
    
    bool exists = false;
    if (PQntuples(result) > 0) {
        std::string value = PQgetvalue(result, 0, 0);
        exists = (value == "t");
    }
    
    PQclear(result);
    return exists;
}

void DatabaseTestHelper::createTestDatabase(PGconn* connection, const std::string &dbName)
{
    std::string query = "CREATE DATABASE " + dbName;
    executeQuery(connection, query);
}

void DatabaseTestHelper::dropTestDatabase(PGconn* connection, const std::string &dbName)
{
    std::string query = "DROP DATABASE IF EXISTS " + dbName;
    executeQuery(connection, query);
}

int DatabaseTestHelper::getRowCount(PGconn* connection, const std::string &tableName)
{
    if (connection == nullptr) {
        throw std::runtime_error("DatabaseTestHelper: Database connection is null");
    }
    
    std::string query = "SELECT COUNT(*) FROM " + tableName;
    
    PGresult* result = PQexec(connection, query.c_str());
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        std::string error = PQerrorMessage(connection);
        PQclear(result);
        throw std::runtime_error("DatabaseTestHelper: Row count query failed: " + error);
    }
    
    int count = 0;
    if (PQntuples(result) > 0) {
        count = std::stoi(PQgetvalue(result, 0, 0));
    }
    
    PQclear(result);
    return count;
}

std::vector<std::string> DatabaseTestHelper::getTableNames(PGconn* connection)
{
    if (connection == nullptr) {
        throw std::runtime_error("DatabaseTestHelper: Database connection is null");
    }
    
    std::string query = "SELECT tablename FROM pg_tables WHERE schemaname = 'public'";
    
    PGresult* result = PQexec(connection, query.c_str());
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        std::string error = PQerrorMessage(connection);
        PQclear(result);
        throw std::runtime_error("DatabaseTestHelper: Table names query failed: " + error);
    }
    
    std::vector<std::string> tableNames;
    int numRows = PQntuples(result);
    
    for (int i = 0; i < numRows; ++i) {
        tableNames.push_back(PQgetvalue(result, i, 0));
    }
    
    PQclear(result);
    return tableNames;
} 