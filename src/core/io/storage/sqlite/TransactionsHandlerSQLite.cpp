#include "TransactionsHandlerSQLite.h"

TransactionsHandlerSQLite::TransactionsHandlerSQLite(
    sqlite3 *dbConnection,
    const string &tableName,
    Logger &logger):

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    // Validate input parameters
    if (dbConnection == nullptr) {
        throw ValueError("TransactionsHandlerSQLite::constructor: Database connection cannot be null.");
    }

    if (tableName.empty()) {
        throw ValueError("TransactionsHandlerSQLite::constructor: Table name cannot be empty.");
    }

    // Create the main table
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (transaction_uuid BLOB NOT NULL, "
                   "transaction_body BLOB NOT NULL, "
                   "transaction_bytes_count INT NOT NULL);";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("TransactionsHandlerSQLite::constructor: Failed to create table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Create unique index on transaction_uuid
    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName
            + "_transaction_uuid_idx on " + mTableName + " (transaction_uuid);";
    SQLiteStatementRAII indexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(indexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("TransactionsHandlerSQLite::constructor: Failed to create unique index on table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "TransactionsHandler initialized: table=" << mTableName;
#endif
}

void TransactionsHandlerSQLite::saveRecord(
    const TransactionUUID &transactionUUID,
    BytesShared transaction,
    size_t transactionBytesCount)
{
    if (!transaction) {
        throw ValueError("TransactionsHandlerSQLite::saveRecord: Transaction data cannot be null.");
    }

    if (transactionBytesCount == 0) {
        throw ValueError("TransactionsHandlerSQLite::saveRecord: Transaction bytes count cannot be zero.");
    }

    string query = "INSERT OR REPLACE INTO " + mTableName +
                   " (transaction_uuid, transaction_body, transaction_bytes_count) VALUES(?, ?, ?);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    // Bind transaction UUID
    int rc = sqlite3_bind_blob(stmt.get(), 1, transactionUUID.data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("TransactionsHandlerSQLite::saveRecord: Failed to bind transaction_uuid. "
                      "BytesCount=" + to_string(transactionBytesCount) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Bind transaction body
    rc = sqlite3_bind_blob(stmt.get(), 2, transaction.get(), (int)transactionBytesCount, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("TransactionsHandlerSQLite::saveRecord: Failed to bind transaction_body. "
                      "BytesCount=" + to_string(transactionBytesCount) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Bind transaction bytes count
    rc = sqlite3_bind_int(stmt.get(), 3, (int)transactionBytesCount);
    if (rc != SQLITE_OK) {
        throw IOError("TransactionsHandlerSQLite::saveRecord: Failed to bind transaction_bytes_count. "
                      "BytesCount=" + to_string(transactionBytesCount) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("TransactionsHandlerSQLite::saveRecord: Failed to execute INSERT OR REPLACE. "
                      "BytesCount=" + to_string(transactionBytesCount) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Transaction saved: BytesCount=" << transactionBytesCount;
#endif
}

void TransactionsHandlerSQLite::deleteRecordIfExists(
    const TransactionUUID &transactionUUID)
{
    string query = "DELETE FROM " + mTableName + " WHERE transaction_uuid = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, transactionUUID.data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("TransactionsHandlerSQLite::deleteRecord: Failed to bind transaction_uuid. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("TransactionsHandlerSQLite::deleteRecord: Failed to execute DELETE. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Transaction deleted successfully";
#endif
}

BytesShared TransactionsHandlerSQLite::getTransaction(
    const TransactionUUID &transactionUUID)
{
    string query = "SELECT transaction_body, transaction_bytes_count FROM "
                   + mTableName + " WHERE transaction_uuid = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, transactionUUID.data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("TransactionsHandlerSQLite::getTransaction: Failed to bind transaction_uuid. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        auto transactionBytesCount = (size_t)sqlite3_column_int(stmt.get(), 1);

        if (transactionBytesCount == 0) {
            throw IOError("TransactionsHandlerSQLite::getTransaction: Invalid transaction bytes count (zero).");
        }

        BytesShared transaction = tryMalloc(transactionBytesCount);
        memcpy(
            transaction.get(),
            sqlite3_column_blob(stmt.get(), 0),
            transactionBytesCount);

#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Transaction retrieved: BytesCount=" << transactionBytesCount;
#endif
        return transaction;
    } else if (rc == SQLITE_DONE) {
        throw NotFoundError("TransactionsHandlerSQLite::getTransaction: No transaction found with the specified UUID.");
    } else {
        throw IOError("TransactionsHandlerSQLite::getTransaction: Failed to execute SELECT. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

bool TransactionsHandlerSQLite::isTransactionSerialized(
    const TransactionUUID &transactionUUID)
{
    string query = "SELECT 1 FROM " + mTableName + " WHERE transaction_uuid = ? LIMIT 1";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, transactionUUID.data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("TransactionsHandlerSQLite::isTransactionSerialized: Failed to bind transaction_uuid. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Transaction found in database";
#endif
        return true;
    } else if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Transaction not found in database";
#endif
        return false;
    } else {
        throw IOError("TransactionsHandlerSQLite::isTransactionSerialized: Failed to execute SELECT. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

vector<BytesShared> TransactionsHandlerSQLite::allTransactions()
{
    vector<BytesShared> result;

    // First, get the count to reserve space
    string countQuery = "SELECT count(*) FROM " + mTableName;
    SQLiteStatementRAII countStmt(mDataBase, countQuery.c_str());

    int rc = sqlite3_step(countStmt.get());
    if (rc == SQLITE_ROW) {
        auto rowCount = (uint32_t)sqlite3_column_int(countStmt.get(), 0);
        result.reserve(rowCount);
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Retrieving all transactions: ExpectedCount=" << rowCount;
#endif
    } else {
        throw IOError("TransactionsHandlerSQLite::allTransactions: Failed to execute count query. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Now get the actual data
    string query = "SELECT transaction_body, transaction_bytes_count FROM " + mTableName + ";";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        auto transactionBytesCount = (size_t)sqlite3_column_int(stmt.get(), 1);

        if (transactionBytesCount == 0) {
            warning() << "Skipping transaction with zero bytes count";
            continue;
        }

        BytesShared transaction = tryMalloc(transactionBytesCount);
        memcpy(
            transaction.get(),
            sqlite3_column_blob(stmt.get(), 0),
            transactionBytesCount);

        result.push_back(transaction);
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "All transactions retrieved: ActualCount=" << result.size();
#endif
    return result;
}

LoggerStream TransactionsHandlerSQLite::info() const
{
    return mLog.info(logHeader());
}

LoggerStream TransactionsHandlerSQLite::warning() const
{
    return mLog.warning(logHeader());
}

const string TransactionsHandlerSQLite::logHeader() const
{
    stringstream s;
    s << "[TransactionsHandler: (" << mTableName << ")]";
    return s.str();
}
