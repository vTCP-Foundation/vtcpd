#include "SQLiteStatementRAII.h"
#include "../../common/exceptions/IOError.h"
#include <string>

SQLiteStatementRAII::SQLiteStatementRAII(sqlite3* db, const char* sql) : mStmt(nullptr)
{
    int rc = sqlite3_prepare_v2(db, sql, -1, &mStmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("SQLiteStatementRAII: Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
    }
}

SQLiteStatementRAII::SQLiteStatementRAII(sqlite3_stmt* stmt) : mStmt(stmt)
{
}

SQLiteStatementRAII::~SQLiteStatementRAII()
{
    cleanup();
}

SQLiteStatementRAII::SQLiteStatementRAII(SQLiteStatementRAII&& other) noexcept : mStmt(other.mStmt)
{
    other.mStmt = nullptr;
}

SQLiteStatementRAII& SQLiteStatementRAII::operator=(SQLiteStatementRAII&& other) noexcept
{
    if (this != &other) {
        cleanup();
        mStmt = other.mStmt;
        other.mStmt = nullptr;
    }
    return *this;
}

sqlite3_stmt* SQLiteStatementRAII::release() noexcept
{
    sqlite3_stmt* temp = mStmt;
    mStmt = nullptr;
    return temp;
}

int SQLiteStatementRAII::reset() noexcept
{
    if (mStmt) {
        sqlite3_reset(mStmt);
        return sqlite3_clear_bindings(mStmt);
    }
    return SQLITE_OK;
}

void SQLiteStatementRAII::cleanup() noexcept
{
    if (mStmt) {
        sqlite3_reset(mStmt);
        sqlite3_finalize(mStmt);
        mStmt = nullptr;
    }
}