#include "PaymentTransactionsHandlerSQLite.h"

PaymentTransactionsHandlerSQLite::PaymentTransactionsHandlerSQLite(
    sqlite3 *dbConnection,
    const string &tableName,
    Logger &logger):

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    // Validate input parameters
    if (dbConnection == nullptr) {
        throw ValueError("PaymentTransactionsHandlerSQLite::constructor: Database connection cannot be null.");
    }

    if (tableName.empty()) {
        throw ValueError("PaymentTransactionsHandlerSQLite::constructor: Table name cannot be empty.");
    }

    // Create main table
    // effective_claiming_block_number stores the extended claiming deadline that includes
    // the dispute grace period. This value equals maximal_claiming_block_number +
    // disputeGracePeriodBlocksCount and is used for observer monitoring queries.
    // BlockNumber columns are stored as INTEGER (not BLOB) to enable correct numeric comparisons.
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (uuid BLOB NOT NULL, "
                   "maximal_claiming_block_number INTEGER NOT NULL, "
                   "effective_claiming_block_number INTEGER NOT NULL, "
                   "observing_state INTEGER NOT NULL, "
                   "recording_time INTEGER NOT NULL, "
                   "payment_key_id INTEGER NOT NULL);";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("PaymentTransactionsHandlerSQLite::constructor: Failed to create table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Create index on UUID for faster lookups
    query = "CREATE INDEX IF NOT EXISTS " + mTableName
            + "_uuid_idx on " + mTableName + " (uuid);";
    // Create index on payment_key_id for faster joins
    query = "CREATE INDEX IF NOT EXISTS " + mTableName
            + "_payment_key_id_idx on " + mTableName + " (payment_key_id);";

    SQLiteStatementRAII indexStmt2(mDataBase, query.c_str());
    rc = sqlite3_step(indexStmt2.get());
    if (rc != SQLITE_DONE) {
        throw IOError("PaymentTransactionsHandlerSQLite::constructor: Failed to create payment_key_id index on table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    SQLiteStatementRAII indexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(indexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("PaymentTransactionsHandlerSQLite::constructor: Failed to create UUID index on table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "PaymentTransactionsHandler initialized: table=" << mTableName;
#endif
}

void PaymentTransactionsHandlerSQLite::saveRecord(
    const TransactionUUID &transactionUUID,
    BlockNumber maximalClaimingBlockNumber,
    BlockNumber effectiveClaimingBlockNumber)
{
    // Insert payment transaction record with both claiming block numbers:
    // - maximal_claiming_block_number: original claiming deadline
    // - effective_claiming_block_number: extended deadline including dispute grace period
    string query = "INSERT INTO " + mTableName + " (uuid, maximal_claiming_block_number, "
                   "effective_claiming_block_number, observing_state, recording_time, payment_key_id) "
                   "VALUES(?, ?, ?, ?, ?, (SELECT id FROM payment_keys ORDER BY id DESC LIMIT 1));";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    // Create a local copy of the UUID data to ensure it remains valid
    // throughout the entire SQLite operation
    uint8_t uuidData[TransactionUUID::kBytesSize];
    memcpy(uuidData, transactionUUID.data, TransactionUUID::kBytesSize);

    int rc = sqlite3_bind_blob(stmt.get(), 1, uuidData, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentTransactionsHandlerSQLite::saveRecord: Failed to bind transaction UUID. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int64(stmt.get(), 2, static_cast<int64_t>(maximalClaimingBlockNumber));
    if (rc != SQLITE_OK) {
        throw IOError("PaymentTransactionsHandlerSQLite::saveRecord: Failed to bind maximal claiming block number. "
                      "BlockNumber=" + to_string(maximalClaimingBlockNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Bind effective claiming block number (includes dispute grace period)
    rc = sqlite3_bind_int64(stmt.get(), 3, static_cast<int64_t>(effectiveClaimingBlockNumber));
    if (rc != SQLITE_OK) {
        throw IOError("PaymentTransactionsHandlerSQLite::saveRecord: Failed to bind effective claiming block number. "
                      "BlockNumber=" + to_string(effectiveClaimingBlockNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Set initial observing state to 0 (uncertain state)
    rc = sqlite3_bind_int(stmt.get(), 4, 0);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentTransactionsHandlerSQLite::saveRecord: Failed to bind observing state. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    GEOEpochTimestamp timestamp = microsecondsSinceGEOEpoch(utc_now());
    rc = sqlite3_bind_int64(stmt.get(), 5, timestamp);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentTransactionsHandlerSQLite::saveRecord: Failed to bind recording timestamp. "
                      "Timestamp=" + to_string(timestamp) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("PaymentTransactionsHandlerSQLite::saveRecord: Failed to execute INSERT. "
                      "BlockNumber=" + to_string(maximalClaimingBlockNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Payment transaction record saved: BlockNumber=" << maximalClaimingBlockNumber
           << ", EffectiveBlockNumber=" << effectiveClaimingBlockNumber
           << ", Timestamp=" << timestamp;
#endif
}

void PaymentTransactionsHandlerSQLite::updateTransactionState(
    const TransactionUUID &transactionUUID,
    PaymentObservingState observingState)
{
    string query = "UPDATE " + mTableName +
                   " SET observing_state = ? WHERE uuid = ?;";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int stateValue = static_cast<int>(observingState);
    int rc = sqlite3_bind_int(stmt.get(), 1, stateValue);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentTransactionsHandlerSQLite::updateTransactionState: Failed to bind observing state. "
                      "State=" + to_string(stateValue) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Create a local copy of the UUID data to ensure it remains valid
    // throughout the entire SQLite operation
    uint8_t uuidData[TransactionUUID::kBytesSize];
    memcpy(uuidData, transactionUUID.data, TransactionUUID::kBytesSize);

    rc = sqlite3_bind_blob(stmt.get(), 2, uuidData, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentTransactionsHandlerSQLite::updateTransactionState: Failed to bind transaction UUID. "
                      "State=" + to_string(stateValue) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("PaymentTransactionsHandlerSQLite::updateTransactionState: Failed to execute UPDATE. "
                      "State=" + to_string(stateValue) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    if (sqlite3_changes(mDataBase) == 0) {
        throw ValueError("PaymentTransactionsHandlerSQLite::updateTransactionState: No rows affected. "
                         "Transaction UUID not found or state unchanged. State=" + to_string(stateValue) + ".");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Transaction state updated: State=" << stateValue;
#endif
}

vector<pair<TransactionUUID, BlockNumber>> PaymentTransactionsHandlerSQLite::transactionsWithUncertainObservingState()
{
    vector<pair<TransactionUUID, BlockNumber>> result;

    // Use constant 0 for uncertain observing state
    string query = "SELECT uuid, maximal_claiming_block_number FROM "
                   + mTableName + " WHERE observing_state = 0";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        TransactionUUID transactionUUID((uint8_t*)sqlite3_column_blob(stmt.get(), 0));

        BlockNumber maximalClaimingBlockNumber = static_cast<BlockNumber>(
            sqlite3_column_int64(stmt.get(), 1));

        result.emplace_back(
            transactionUUID,
            maximalClaimingBlockNumber);
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Uncertain state transactions retrieved: Count=" << result.size();
#endif

    return result;
}

vector<pair<TransactionUUID, BlockNumber>> PaymentTransactionsHandlerSQLite::transactionsForObserverMonitoring(
    BlockNumber minBlockNumber,
    uint32_t limit)
{
    vector<pair<TransactionUUID, BlockNumber>> result;

    // Fetch transactions still within effective claiming window (including dispute grace period)
    // with uncertain observing state. The effective_claiming_block_number already includes
    // the dispute grace period, so we compare directly against the current block number.
    string query = "SELECT uuid, maximal_claiming_block_number FROM " + mTableName +
                   " WHERE effective_claiming_block_number > ? AND observing_state = 1 "
                   "ORDER BY maximal_claiming_block_number ASC LIMIT ?;";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int64(stmt.get(), 1, static_cast<int64_t>(minBlockNumber));
    if (rc != SQLITE_OK) {
        throw IOError("PaymentTransactionsHandlerSQLite::transactionsForObserverMonitoring: Failed to bind min block number. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, static_cast<int>(limit));
    if (rc != SQLITE_OK) {
        throw IOError("PaymentTransactionsHandlerSQLite::transactionsForObserverMonitoring: Failed to bind limit. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    while (true) {
        rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_ROW) {
            TransactionUUID transactionUUID((uint8_t*)sqlite3_column_blob(stmt.get(), 0));

            BlockNumber maximalClaimingBlockNumber = static_cast<BlockNumber>(
                sqlite3_column_int64(stmt.get(), 1));

            result.emplace_back(
                transactionUUID,
                maximalClaimingBlockNumber);
        } else if (rc == SQLITE_DONE) {
            break;
        } else {
            throw IOError("PaymentTransactionsHandlerSQLite::transactionsForObserverMonitoring: Failed to execute SELECT. "
                          "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
        }
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Observer monitoring transactions retrieved: Count=" << result.size();
#endif

    return result;
}

bool PaymentTransactionsHandlerSQLite::isTransactionPresent(
    const TransactionUUID &transactionUUID)
{
    string query = "SELECT COUNT(*) FROM "
                   + mTableName + " WHERE uuid = ?";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    // Create a local copy of the UUID data to ensure it remains valid
    // throughout the entire SQLite operation
    uint8_t uuidData[TransactionUUID::kBytesSize];
    memcpy(uuidData, transactionUUID.data, TransactionUUID::kBytesSize);

    int rc = sqlite3_bind_blob(stmt.get(), 1, uuidData, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentTransactionsHandlerSQLite::isTransactionPresent: Failed to bind transaction UUID. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        bool result = (sqlite3_column_int(stmt.get(), 0) > 0);
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Transaction presence checked: Present=" << (result ? "true" : "false");
#endif
        return result;
    } else {
        throw IOError("PaymentTransactionsHandlerSQLite::isTransactionPresent: Failed to execute SELECT. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

void PaymentTransactionsHandlerSQLite::deleteRecord(
    const TransactionUUID &transactionUUID)
{
    string query = "DELETE FROM " + mTableName + " WHERE uuid = ?;";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    // Create a local copy of the UUID data to ensure it remains valid
    // throughout the entire SQLite operation
    uint8_t uuidData[TransactionUUID::kBytesSize];
    memcpy(uuidData, transactionUUID.data, TransactionUUID::kBytesSize);

    int rc = sqlite3_bind_blob(stmt.get(), 1, uuidData, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentTransactionsHandlerSQLite::deleteRecord: Failed to bind transaction UUID. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("PaymentTransactionsHandlerSQLite::deleteRecord: Failed to execute DELETE. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    int deletedRows = sqlite3_changes(mDataBase);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Transaction record deleted: DeletedCount=" << deletedRows;
#endif
}

vector<TransactionUUID> PaymentTransactionsHandlerSQLite::allTransactionsUUID()
{
    vector<TransactionUUID> result;

    string query = "SELECT uuid FROM " + mTableName;

    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        TransactionUUID transactionUUID((uint8_t*)sqlite3_column_blob(stmt.get(), 0));
        result.emplace_back(transactionUUID);
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "All transaction UUIDs retrieved: Count=" << result.size();
#endif

    return result;
}

LoggerStream PaymentTransactionsHandlerSQLite::info() const
{
    return mLog.info(logHeader());
}

LoggerStream PaymentTransactionsHandlerSQLite::warning() const
{
    return mLog.warning(logHeader());
}

const string PaymentTransactionsHandlerSQLite::logHeader() const
{
    stringstream s;
    s << "[PaymentTransactionsHandler: (" << mTableName << ")]";
    return s.str();
}
