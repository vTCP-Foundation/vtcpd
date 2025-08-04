#ifndef VTCPD_MOCK_SQLITE_API_H
#define VTCPD_MOCK_SQLITE_API_H

#include <gmock/gmock.h>
#include <sqlite3.h>
#include <string>
#include <memory>

using namespace std;

/**
 * Mock interface for SQLite API functions.
 * This interface wraps all SQLite functions used in the project
 * to enable unit testing with mocked database operations.
 */
class ISQLiteAPI {
public:
    virtual ~ISQLiteAPI() = default;

    // Database connection management
    virtual int sqlite3_open_v2(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs) = 0;
    virtual int sqlite3_close_v2(sqlite3 *db) = 0;
    virtual int sqlite3_config(int option, ...) = 0;
    virtual int sqlite3_db_config(sqlite3 *db, int op, ...) = 0;

    // Statement preparation and execution
    virtual int sqlite3_prepare_v2(sqlite3 *db, const char *zSql, int nByte, sqlite3_stmt **ppStmt, const char **pzTail) = 0;
    virtual int sqlite3_step(sqlite3_stmt *stmt) = 0;
    virtual int sqlite3_finalize(sqlite3_stmt *stmt) = 0;
    virtual int sqlite3_reset(sqlite3_stmt *stmt) = 0;
    virtual int sqlite3_clear_bindings(sqlite3_stmt *stmt) = 0;
    virtual int sqlite3_exec(sqlite3 *db, const char *sql, int (*callback)(void*,int,char**,char**), void *arg, char **errmsg) = 0;

    // Parameter binding
    virtual int sqlite3_bind_blob(sqlite3_stmt *stmt, int idx, const void *value, int n, void(*destructor)(void*)) = 0;
    virtual int sqlite3_bind_int(sqlite3_stmt *stmt, int idx, int value) = 0;
    virtual int sqlite3_bind_int64(sqlite3_stmt *stmt, int idx, sqlite3_int64 value) = 0;
    virtual int sqlite3_bind_text(sqlite3_stmt *stmt, int idx, const char *value, int n, void(*destructor)(void*)) = 0;
    virtual int sqlite3_bind_null(sqlite3_stmt *stmt, int idx) = 0;

    // Result retrieval
    virtual const void* sqlite3_column_blob(sqlite3_stmt *stmt, int idx) = 0;
    virtual int sqlite3_column_bytes(sqlite3_stmt *stmt, int idx) = 0;
    virtual int sqlite3_column_int(sqlite3_stmt *stmt, int idx) = 0;
    virtual sqlite3_int64 sqlite3_column_int64(sqlite3_stmt *stmt, int idx) = 0;
    virtual const unsigned char* sqlite3_column_text(sqlite3_stmt *stmt, int idx) = 0;
    virtual int sqlite3_column_type(sqlite3_stmt *stmt, int idx) = 0;

    // Error handling and metadata
    virtual const char* sqlite3_errmsg(sqlite3 *db) = 0;
    virtual int sqlite3_changes(sqlite3 *db) = 0;
    virtual sqlite3_int64 sqlite3_last_insert_rowid(sqlite3 *db) = 0;
    virtual int sqlite3_db_release_memory(sqlite3 *db) = 0;
};

/**
 * Mock implementation of SQLite API using Google Mock.
 * This class provides mock implementations for all SQLite functions
 * to enable isolated unit testing of database-dependent code.
 */
class MockSQLiteAPI : public ISQLiteAPI {
public:
    // Database connection management
    MOCK_METHOD(int, sqlite3_open_v2, (const char *filename, sqlite3 **ppDb, int flags, const char *zVfs), (override));
    MOCK_METHOD(int, sqlite3_close_v2, (sqlite3 *db), (override));
    MOCK_METHOD(int, sqlite3_config, (int option, ...), (override));
    MOCK_METHOD(int, sqlite3_db_config, (sqlite3 *db, int op, ...), (override));

    // Statement preparation and execution
    MOCK_METHOD(int, sqlite3_prepare_v2, (sqlite3 *db, const char *zSql, int nByte, sqlite3_stmt **ppStmt, const char **pzTail), (override));
    MOCK_METHOD(int, sqlite3_step, (sqlite3_stmt *stmt), (override));
    MOCK_METHOD(int, sqlite3_finalize, (sqlite3_stmt *stmt), (override));
    MOCK_METHOD(int, sqlite3_reset, (sqlite3_stmt *stmt), (override));
    MOCK_METHOD(int, sqlite3_clear_bindings, (sqlite3_stmt *stmt), (override));
    MOCK_METHOD(int, sqlite3_exec, (sqlite3 *db, const char *sql, int (*callback)(void*,int,char**,char**), void *arg, char **errmsg), (override));

    // Parameter binding
    MOCK_METHOD(int, sqlite3_bind_blob, (sqlite3_stmt *stmt, int idx, const void *value, int n, void(*destructor)(void*)), (override));
    MOCK_METHOD(int, sqlite3_bind_int, (sqlite3_stmt *stmt, int idx, int value), (override));
    MOCK_METHOD(int, sqlite3_bind_int64, (sqlite3_stmt *stmt, int idx, sqlite3_int64 value), (override));
    MOCK_METHOD(int, sqlite3_bind_text, (sqlite3_stmt *stmt, int idx, const char *value, int n, void(*destructor)(void*)), (override));
    MOCK_METHOD(int, sqlite3_bind_null, (sqlite3_stmt *stmt, int idx), (override));

    // Result retrieval
    MOCK_METHOD(const void*, sqlite3_column_blob, (sqlite3_stmt *stmt, int idx), (override));
    MOCK_METHOD(int, sqlite3_column_bytes, (sqlite3_stmt *stmt, int idx), (override));
    MOCK_METHOD(int, sqlite3_column_int, (sqlite3_stmt *stmt, int idx), (override));
    MOCK_METHOD(sqlite3_int64, sqlite3_column_int64, (sqlite3_stmt *stmt, int idx), (override));
    MOCK_METHOD(const unsigned char*, sqlite3_column_text, (sqlite3_stmt *stmt, int idx), (override));
    MOCK_METHOD(int, sqlite3_column_type, (sqlite3_stmt *stmt, int idx), (override));

    // Error handling and metadata
    MOCK_METHOD(const char*, sqlite3_errmsg, (sqlite3 *db), (override));
    MOCK_METHOD(int, sqlite3_changes, (sqlite3 *db), (override));
    MOCK_METHOD(sqlite3_int64, sqlite3_last_insert_rowid, (sqlite3 *db), (override));
    MOCK_METHOD(int, sqlite3_db_release_memory, (sqlite3 *db), (override));
};

/**
 * Mock statement wrapper for SQLite statements.
 * This class provides a mock implementation of sqlite3_stmt
 * that can be used in unit tests.
 */
class MockSQLiteStatement {
public:
    MockSQLiteStatement() = default;
    virtual ~MockSQLiteStatement() = default;

    // Convert to sqlite3_stmt* for compatibility
    operator sqlite3_stmt*() { return reinterpret_cast<sqlite3_stmt*>(this); }
    sqlite3_stmt* get() { return reinterpret_cast<sqlite3_stmt*>(this); }
};

/**
 * Mock database connection wrapper.
 * This class provides a mock implementation of sqlite3 database connection
 * that can be used in unit tests.
 */
class MockSQLiteConnection {
public:
    MockSQLiteConnection() = default;
    virtual ~MockSQLiteConnection() = default;

    // Convert to sqlite3* for compatibility
    operator sqlite3*() { return reinterpret_cast<sqlite3*>(this); }
    sqlite3* get() { return reinterpret_cast<sqlite3*>(this); }
};

/**
 * Helper class for setting up common mock expectations.
 * This class provides convenience methods for setting up
 * frequently used mock expectations patterns.
 */
class MockExpectationsHelper {
public:
    // Table creation expectations
    static void expectSuccessfulTableCreation(MockSQLiteAPI& mockAPI, const std::string& tableName);
    static void expectFailedTableCreation(MockSQLiteAPI& mockAPI, const std::string& tableName, int errorCode);

    // Index creation expectations
    static void expectSuccessfulIndexCreation(MockSQLiteAPI& mockAPI, const std::string& indexName);
    static void expectFailedIndexCreation(MockSQLiteAPI& mockAPI, const std::string& indexName, int errorCode);

    // Insert operation expectations
    static void expectSuccessfulInsert(MockSQLiteAPI& mockAPI);
    static void expectFailedInsert(MockSQLiteAPI& mockAPI, int errorCode);

    // Select operation expectations
    static void expectSuccessfulSelect(MockSQLiteAPI& mockAPI);
    static void expectEmptySelect(MockSQLiteAPI& mockAPI);
    static void expectFailedSelect(MockSQLiteAPI& mockAPI, int errorCode);

    // Update operation expectations
    static void expectSuccessfulUpdate(MockSQLiteAPI& mockAPI);
    static void expectFailedUpdate(MockSQLiteAPI& mockAPI, int errorCode);

    // Delete operation expectations
    static void expectSuccessfulDelete(MockSQLiteAPI& mockAPI);
    static void expectFailedDelete(MockSQLiteAPI& mockAPI, int errorCode);

    // Connection management expectations
    static void expectSuccessfulConnection(MockSQLiteAPI& mockAPI);
    static void expectFailedConnection(MockSQLiteAPI& mockAPI, int errorCode);

    // Parameter binding expectations
    static void expectSuccessfulBind(MockSQLiteAPI& mockAPI, int parameterCount);
    static void expectFailedBind(MockSQLiteAPI& mockAPI, int parameterIndex, int errorCode);

    // Error message expectations
    static void expectErrorMessage(MockSQLiteAPI& mockAPI, const std::string& message);
};

#endif // VTCPD_MOCK_SQLITE_API_H 