#include "OutgoingPaymentReceiptHandler.h"

OutgoingPaymentReceiptHandler::OutgoingPaymentReceiptHandler(
    sqlite3 *dbConnection,
    const string &tableName,
    Logger &logger) :

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    // Validate input parameters
    if (dbConnection == nullptr) {
        throw ValueError("OutgoingPaymentReceiptHandler::constructor: Database connection cannot be null.");
    }

    if (tableName.empty()) {
        throw ValueError("OutgoingPaymentReceiptHandler::constructor: Table name cannot be empty.");
    }

    // Create the main table
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (trust_line_id INTEGER NOT NULL, "
                   "audit_number INTEGER NOT NULL, "
                   "transaction_uuid BLOB NOT NULL, "
                   "own_public_key_hash BLOB NOT NULL, "
                   "amount BLOB NOT NULL, "
                   "FOREIGN KEY(trust_line_id) REFERENCES trust_lines(id) ON DELETE CASCADE ON UPDATE CASCADE, "
                   "FOREIGN KEY(own_public_key_hash) REFERENCES own_keys(hash) ON DELETE CASCADE ON UPDATE CASCADE);";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("OutgoingPaymentReceiptHandler::constructor: Failed to create table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Create unique index on trust_line_id, audit_number, and own_public_key_hash
    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName + "_trust_line_id_audit_number_key_hash_idx on " +
            mTableName + "(trust_line_id, audit_number, own_public_key_hash);";
    SQLiteStatementRAII uniqueIndexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(uniqueIndexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("OutgoingPaymentReceiptHandler::constructor: Failed to create unique index on table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Create index on transaction_uuid for faster lookups
    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_transaction_uuid_idx on " + mTableName + "(transaction_uuid);";
    SQLiteStatementRAII transactionIndexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(transactionIndexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("OutgoingPaymentReceiptHandler::constructor: Failed to create transaction_uuid index on table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "OutgoingPaymentReceiptHandler initialized: table=" << mTableName;
#endif
}

void OutgoingPaymentReceiptHandler::saveRecord(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber,
    const TransactionUUID &transactionUUID,
    const KeyHash::Shared ownPublicKeyHash,
    const TrustLineAmount &amount)
{
    if (!ownPublicKeyHash) {
        throw ValueError("OutgoingPaymentReceiptHandler::saveRecord: Own public key hash cannot be null.");
    }

    string query = "INSERT INTO " + mTableName +
                   "(trust_line_id, audit_number, transaction_uuid, own_public_key_hash, "
                   "amount) VALUES (?, ?, ?, ?, ?);";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    // Bind parameters
    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("OutgoingPaymentReceiptHandler::saveRecord: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, auditNumber);
    if (rc != SQLITE_OK) {
        throw IOError("OutgoingPaymentReceiptHandler::saveRecord: Failed to bind audit_number. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_blob(stmt.get(), 3, transactionUUID.data,
                           (int)TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("OutgoingPaymentReceiptHandler::saveRecord: Failed to bind transaction_uuid. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_blob(stmt.get(), 4, ownPublicKeyHash->data(),
                           (int)KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("OutgoingPaymentReceiptHandler::saveRecord: Failed to bind own_public_key_hash. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    vector<byte_t> amountBufferBytes = trustLineAmountToBytes(amount);
    rc = sqlite3_bind_blob(stmt.get(), 5, amountBufferBytes.data(), kTrustLineAmountBytesCount, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        throw IOError("OutgoingPaymentReceiptHandler::saveRecord: Failed to bind amount. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("OutgoingPaymentReceiptHandler::saveRecord: Failed to execute INSERT. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Receipt record saved: TrustLine=" << trustLineID
           << ", AuditNumber=" << auditNumber
           << ", Amount=" << amount;
#endif
}

vector<pair<TransactionUUID, TrustLineAmount>> OutgoingPaymentReceiptHandler::auditAmounts(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber)
{
    vector<pair<TransactionUUID, TrustLineAmount>> result;

    string query = "SELECT transaction_uuid, amount FROM " + mTableName + " WHERE trust_line_id = ? AND audit_number = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("OutgoingPaymentReceiptHandler::auditAmounts: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, auditNumber);
    if (rc != SQLITE_OK) {
        throw IOError("OutgoingPaymentReceiptHandler::auditAmounts: Failed to bind audit_number. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        TransactionUUID transactionUUID((uint8_t*)sqlite3_column_blob(stmt.get(), 0));

        auto amountBytes = (byte_t*)sqlite3_column_blob(stmt.get(), 1);
        vector<byte_t> amountBufferBytes(
            amountBytes,
            amountBytes + kTrustLineAmountBytesCount);
        auto amount = bytesToTrustLineAmount(amountBufferBytes);

        result.emplace_back(transactionUUID, amount);
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Audit amounts retrieved: TrustLine=" << trustLineID
           << ", AuditNumber=" << auditNumber
           << ", Count=" << result.size();
#endif

    return result;
}

vector<ReceiptRecord::Shared> OutgoingPaymentReceiptHandler::receiptsByAuditNumber(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber)
{
    vector<ReceiptRecord::Shared> result;

    string query = "SELECT amount, transaction_uuid, own_public_key_hash FROM " + mTableName +
                   " WHERE trust_line_id = ? AND audit_number = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("OutgoingPaymentReceiptHandler::receiptsByAuditNumber: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, auditNumber);
    if (rc != SQLITE_OK) {
        throw IOError("OutgoingPaymentReceiptHandler::receiptsByAuditNumber: Failed to bind audit_number. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        auto amountBytes = (byte_t*)sqlite3_column_blob(stmt.get(), 0);
        vector<byte_t> incomingAmountBufferBytes(
            amountBytes,
            amountBytes + kTrustLineAmountBytesCount);

        TransactionUUID transactionUUID((uint8_t*)sqlite3_column_blob(stmt.get(), 1));

        auto ownKeyHash = make_shared<lamport::KeyHash>(
                              (byte_t*)sqlite3_column_blob(stmt.get(), 2));

        result.push_back(make_shared<ReceiptRecord>(
                             auditNumber,
                             transactionUUID,
                             bytesToTrustLineAmount(incomingAmountBufferBytes),
                             ownKeyHash,
                             nullptr));
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Receipts by audit number retrieved: TrustLine=" << trustLineID
           << ", AuditNumber=" << auditNumber
           << ", Count=" << result.size();
#endif

    return result;
}

vector<ReceiptRecord::Shared> OutgoingPaymentReceiptHandler::receiptsLessEqualThanAuditNumber(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber)
{
    vector<ReceiptRecord::Shared> result;

    string query = "SELECT amount, transaction_uuid, own_public_key_hash, audit_number FROM " + mTableName +
                   " WHERE trust_line_id = ? AND audit_number <= ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("OutgoingPaymentReceiptHandler::receiptsLessEqualThanAuditNumber: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, auditNumber);
    if (rc != SQLITE_OK) {
        throw IOError("OutgoingPaymentReceiptHandler::receiptsLessEqualThanAuditNumber: Failed to bind audit_number. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        auto amountBytes = (byte_t*)sqlite3_column_blob(stmt.get(), 0);
        vector<byte_t> incomingAmountBufferBytes(
            amountBytes,
            amountBytes + kTrustLineAmountBytesCount);

        TransactionUUID transactionUUID((uint8_t*)sqlite3_column_blob(stmt.get(), 1));

        auto ownKeyHash = make_shared<lamport::KeyHash>(
                              (byte_t*)sqlite3_column_blob(stmt.get(), 2));

        auto currentAuditNumber = (AuditNumber)sqlite3_column_int(stmt.get(), 3);

        result.push_back(make_shared<ReceiptRecord>(
                             currentAuditNumber,
                             transactionUUID,
                             bytesToTrustLineAmount(incomingAmountBufferBytes),
                             ownKeyHash,
                             nullptr));
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Receipts less equal than audit number retrieved: TrustLine=" << trustLineID
           << ", MaxAuditNumber=" << auditNumber
           << ", Count=" << result.size();
#endif

    return result;
}

uint32_t OutgoingPaymentReceiptHandler::countReceiptsByNumber(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber)
{
    string query = "SELECT COUNT(transaction_uuid) FROM " + mTableName + " WHERE trust_line_id = ? AND audit_number = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("OutgoingPaymentReceiptHandler::countReceiptsByNumber: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, auditNumber);
    if (rc != SQLITE_OK) {
        throw IOError("OutgoingPaymentReceiptHandler::countReceiptsByNumber: Failed to bind audit_number. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        auto countReceipts = (uint32_t)sqlite3_column_int(stmt.get(), 0);

#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Receipt count retrieved: TrustLine=" << trustLineID
               << ", AuditNumber=" << auditNumber
               << ", Count=" << countReceipts;
#endif
        return countReceipts;
    } else {
        throw IOError("OutgoingPaymentReceiptHandler::countReceiptsByNumber: Failed to execute COUNT query. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

void OutgoingPaymentReceiptHandler::deleteRecords(
    const TransactionUUID &transactionUUID)
{
    string query = "DELETE FROM " + mTableName + " WHERE transaction_uuid = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, transactionUUID.data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("OutgoingPaymentReceiptHandler::deleteRecords: Failed to bind transaction_uuid. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("OutgoingPaymentReceiptHandler::deleteRecords: Failed to execute DELETE by transaction. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    int deletedRows = sqlite3_changes(mDataBase);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Records deleted by transaction UUID: DeletedCount=" << deletedRows;
#endif
}

void OutgoingPaymentReceiptHandler::deleteRecords(
    const TrustLineID trustLineID)
{
    string query = "DELETE FROM " + mTableName + " WHERE trust_line_id = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("OutgoingPaymentReceiptHandler::deleteRecords: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("OutgoingPaymentReceiptHandler::deleteRecords: Failed to execute DELETE by trust line. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    int deletedRows = sqlite3_changes(mDataBase);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Records deleted by trust line: TrustLine=" << trustLineID
           << ", DeletedCount=" << deletedRows;
#endif
}

void OutgoingPaymentReceiptHandler::deleteRecords(
    KeyHash::Shared keyHash)
{
    if (!keyHash) {
        throw ValueError("OutgoingPaymentReceiptHandler::deleteRecords: Key hash cannot be null.");
    }

    string query = "DELETE FROM " + mTableName + " WHERE own_public_key_hash = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, keyHash->data(),
                               (int)KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("OutgoingPaymentReceiptHandler::deleteRecords: Failed to bind own_public_key_hash. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("OutgoingPaymentReceiptHandler::deleteRecords: Failed to execute DELETE by key hash. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    int deletedRows = sqlite3_changes(mDataBase);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Records deleted by key hash: DeletedCount=" << deletedRows;
#endif
}

bool OutgoingPaymentReceiptHandler::isContainsKeyHash(
    KeyHash::Shared keyHash) const
{
    if (!keyHash) {
        throw ValueError("OutgoingPaymentReceiptHandler::isContainsKeyHash: Key hash cannot be null.");
    }

    string query = "SELECT own_public_key_hash FROM " + mTableName + " WHERE own_public_key_hash = ? LIMIT 1";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, keyHash->data(),
                               (int)KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("OutgoingPaymentReceiptHandler::isContainsKeyHash: Failed to bind own_public_key_hash. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    bool result = (sqlite3_step(stmt.get()) == SQLITE_ROW);

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Key hash existence check: Found=" << (result ? "true" : "false");
#endif

    return result;
}

bool OutgoingPaymentReceiptHandler::isContainsTransaction(
    const TransactionUUID &transactionUUID) const
{
    string query = "SELECT transaction_uuid FROM " + mTableName + " WHERE transaction_uuid = ? LIMIT 1";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, transactionUUID.data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("OutgoingPaymentReceiptHandler::isContainsTransaction: Failed to bind transaction_uuid. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    bool result = (sqlite3_step(stmt.get()) == SQLITE_ROW);

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Transaction existence check: Found=" << (result ? "true" : "false");
#endif

    return result;
}

LoggerStream OutgoingPaymentReceiptHandler::info() const
{
    return mLog.info(logHeader());
}

LoggerStream OutgoingPaymentReceiptHandler::warning() const
{
    return mLog.warning(logHeader());
}

const string OutgoingPaymentReceiptHandler::logHeader() const
{
    stringstream s;
    s << "[OutgoingPaymentReceiptHandler: (" << mTableName << ")]";
    return s.str();
}