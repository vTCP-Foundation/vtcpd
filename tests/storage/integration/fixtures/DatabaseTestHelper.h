#ifndef VTCPD_DATABASETESTHELPER_H
#define VTCPD_DATABASETESTHELPER_H

#include <libpq-fe.h>
#include <string>
#include <vector>

class DatabaseTestHelper
{
public:
    // Database connection management
    static PGconn* createConnection(
        const std::string &host,
        int port,
        const std::string &user,
        const std::string &password,
        const std::string &dbName);

    static void closeConnection(PGconn* connection);

    // Database cleanup operations
    static void cleanupTable(PGconn* connection, const std::string &tableName);
    static void cleanupAllTables(PGconn* connection);

    // Utility methods for test setup
    static void executeQuery(PGconn* connection, const std::string &query);
    static bool tableExists(PGconn* connection, const std::string &tableName);
    static void createTestDatabase(PGconn* connection, const std::string &dbName);
    static void dropTestDatabase(PGconn* connection, const std::string &dbName);

    // Test data verification
    static int getRowCount(PGconn* connection, const std::string &tableName);
    static std::vector<std::string> getTableNames(PGconn* connection);

    // Database connection parameters for tests
    static const std::string TEST_HOST;
    static const int TEST_PORT;
    static const std::string TEST_USER;
    static const std::string TEST_PASSWORD;
    static const std::string TEST_DB_NAME;
};

#endif // VTCPD_DATABASETESTHELPER_H 