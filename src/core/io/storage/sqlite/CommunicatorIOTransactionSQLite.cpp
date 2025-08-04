#include "CommunicatorIOTransactionSQLite.h"

CommunicatorIOTransactionSQLite::CommunicatorIOTransactionSQLite(
    sqlite3 *dbConnection,
    CommunicatorMessagesQueueHandler *communicatorMessagesQueueHandler,
    Logger &logger) :

    mDBConnection(dbConnection),
    mCommunicatorMessagesQueueHandler(communicatorMessagesQueueHandler),
    mIsTransactionBegin(true),
    mLog(logger)
{
    beginTransactionQuery();
}

CommunicatorIOTransactionSQLite::~CommunicatorIOTransactionSQLite()
{
    commit();
}

CommunicatorMessagesQueueHandler* CommunicatorIOTransactionSQLite::communicatorMessagesQueueHandler()
{
    if (!mIsTransactionBegin) {
        throw IOError("CommunicatorIOTransactionSQLite::communicatorMessagesQueueHandler: "
                      "transaction was rollback, it can't be use now");
    }
    return mCommunicatorMessagesQueueHandler;
}

void CommunicatorIOTransactionSQLite::commit()
{
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "commit";
#endif
    if (!mIsTransactionBegin) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "transaction don't commit it was rollbacked";
#endif
        return;
    }
    string query = "COMMIT TRANSACTION;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2( mDBConnection, query.c_str(), -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        throw IOError("CommunicatorIOTransactionSQLite::commit: Bad query; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        throw IOError("CommunicatorIOTransactionSQLite::commit: Run query; sqlite error: " + to_string(rc));
    }
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "transaction commit";
#endif
}

void CommunicatorIOTransactionSQLite::rollback()
{
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "rollback";
#endif
    string query = "ROLLBACK TRANSACTION;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDBConnection, query.c_str(), -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        throw IOError("CommunicatorIOTransactionSQLite::rollback: Bad query; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        throw IOError("CommunicatorIOTransactionSQLite::rollback: Run query; sqlite error: " + to_string(rc));
    }
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "rollBack done";
#endif
    mIsTransactionBegin = false;
}

LoggerStream CommunicatorIOTransactionSQLite::info() const
{
    return mLog.info(logHeader());
}

LoggerStream CommunicatorIOTransactionSQLite::warning() const
{
    return mLog.warning(logHeader());
}

const string CommunicatorIOTransactionSQLite::logHeader() const
{
    stringstream s;
    s << "[CommunicatorIOTransaction]";
    return s.str();
}

void CommunicatorIOTransactionSQLite::beginTransactionQuery()
{
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "beginTransactionQuery";
#endif
    string query = "BEGIN TRANSACTION;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDBConnection, query.c_str(), -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        throw IOError("CommunicatorIOTransactionSQLite::prepareInserted: Bad query; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        throw IOError("CommunicatorIOTransactionSQLite::prepareInserted: Run query; sqlite error: " + to_string(rc));
    }
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "transaction begin";
#endif
}
