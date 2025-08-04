#ifndef VTCPD_SQLITESTATEMENTRAII_H
#define VTCPD_SQLITESTATEMENTRAII_H

#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/ValueError.h"
#include <sqlite3.h>
#include <memory>

/**
 * RAII wrapper for SQLite prepared statements.
 * Automatically finalizes the statement when the object is destroyed.
 * Provides safe access to statement operations with automatic resource management.
 */
class SQLiteStatementRAII
{
public:
    /**
     * Constructs a prepared statement wrapper.
     * @param dbConnection Database connection (must not be null)
     * @param query SQL query string
     * @throws ValueError if dbConnection is null or query is empty
     * @throws IOError if statement preparation fails
     */
    SQLiteStatementRAII(
        sqlite3 *dbConnection,
        const char *query);

    /**
     * Constructs a wrapper for an existing statement.
     * @param stmt Existing SQLite statement handle
     */
    explicit SQLiteStatementRAII(sqlite3_stmt* stmt = nullptr);

    /**
     * Destructor that automatically finalizes the statement.
     */
    ~SQLiteStatementRAII();

    /**
     * Copy constructor is deleted to prevent statement handle duplication.
     */
    SQLiteStatementRAII(const SQLiteStatementRAII&) = delete;

    /**
     * Assignment operator is deleted to prevent statement handle duplication.
     */
    SQLiteStatementRAII& operator=(const SQLiteStatementRAII&) = delete;

    /**
     * Move constructor.
     */
    SQLiteStatementRAII(SQLiteStatementRAII&& other) noexcept;

    /**
     * Move assignment operator.
     */
    SQLiteStatementRAII& operator=(SQLiteStatementRAII&& other) noexcept;

    /**
     * Provides direct access to the underlying sqlite3_stmt.
     * @return Pointer to the SQLite statement handle
     */
    sqlite3_stmt* statement();

    /**
     * Legacy method name for backward compatibility.
     * @return Pointer to the SQLite statement handle
     */
    sqlite3_stmt* get();

    /**
     * Conversion operator for direct use in SQLite API calls.
     * @return Pointer to the SQLite statement handle
     */
    operator sqlite3_stmt*();

    /**
     * Explicitly releases ownership of the statement without finalizing it.
     * @return Pointer to the statement handle (caller becomes responsible for cleanup)
     */
    sqlite3_stmt* release() noexcept;

    /**
     * Resets and clears bindings of the statement.
     * @return SQLite result code
     */
    int reset() noexcept;

private:
    /**
     * Cleans up the statement by resetting and finalizing it.
     */
    void cleanup() noexcept;

    sqlite3_stmt *mStmt = nullptr;
    bool mIsReleased = false;
};

#endif //VTCPD_SQLITESTATEMENTRAII_H 
