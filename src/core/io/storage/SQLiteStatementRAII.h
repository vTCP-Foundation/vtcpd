#ifndef VTCPD_SQLITESTATEMENTRAII_H
#define VTCPD_SQLITESTATEMENTRAII_H

#include <sqlite3.h>

/**
 * RAII wrapper for SQLite statements to ensure automatic cleanup and prevent memory leaks.
 *
 * This class provides exception-safe management of sqlite3_stmt resources by:
 * - Automatically calling sqlite3_finalize in the destructor
 * - Preventing copy construction and assignment to avoid double-free
 * - Providing move semantics for efficient transfer of ownership
 * - Offering convenient access to the underlying statement pointer
 *
 * Usage example:
 *
 * ```cpp
 * SQLiteStatementRAII stmt(database, "SELECT * FROM table WHERE id = ?");
 * sqlite3_bind_int(stmt.get(), 1, someId);
 * while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
 *     // Process results
 * }
 * // Statement automatically finalized when stmt goes out of scope
 * ```
 *
 */
class SQLiteStatementRAII
{
public:
    /**
     * Constructs and prepares a SQLite statement.
     *
     * @param db SQLite database connection
     * @param sql SQL query string
     * @throws IOError if statement preparation fails
     */
    SQLiteStatementRAII(sqlite3* db, const char* sql);

    /**
     * Constructs with an already prepared statement (takes ownership).
     *
     * @param stmt Prepared SQLite statement (can be nullptr)
     */
    explicit SQLiteStatementRAII(sqlite3_stmt* stmt = nullptr);

    /**
     * Destructor - automatically finalizes the statement if not null.
     */
    ~SQLiteStatementRAII();

    // Disable copy constructor and copy assignment
    SQLiteStatementRAII(const SQLiteStatementRAII&) = delete;
    SQLiteStatementRAII& operator=(const SQLiteStatementRAII&) = delete;

    // Enable move constructor and move assignment
    SQLiteStatementRAII(SQLiteStatementRAII&& other) noexcept;
    SQLiteStatementRAII& operator=(SQLiteStatementRAII&& other) noexcept;

    /**
     * Returns the underlying sqlite3_stmt pointer.
     *
     * @return Raw SQLite statement pointer (may be nullptr)
     */
    sqlite3_stmt* get() const noexcept
    {
        return mStmt;
    }

    /**
     * Returns the underlying sqlite3_stmt pointer for direct use with SQLite functions.
     *
     * @return Raw SQLite statement pointer (may be nullptr)
     */
    operator sqlite3_stmt*() const noexcept
    {
        return mStmt;
    }

    /**
     * Checks if the statement is valid (not nullptr).
     *
     * @return true if statement is not nullptr, false otherwise
     */
    bool is_valid() const noexcept
    {
        return mStmt != nullptr;
    }

    /**
     * Explicitly releases ownership of the statement without finalizing it.
     *
     * @return The raw statement pointer (caller takes ownership)
     */
    sqlite3_stmt* release() noexcept;

    /**
     * Resets and clears bindings of the statement.
     *
     * @return SQLITE_OK on success, error code otherwise
     */
    int reset() noexcept;

private:
    sqlite3_stmt* mStmt;

    void cleanup() noexcept;
};

#endif // VTCPD_SQLITESTATEMENTRAII_H 