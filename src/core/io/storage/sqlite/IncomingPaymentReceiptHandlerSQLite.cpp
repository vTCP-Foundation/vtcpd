#include "IncomingPaymentReceiptHandlerSQLite.h"
#include "../interfaces/PaymentTransactionsHandler.h"
#include "StorageHandlerSQLite.h"



IncomingPaymentReceiptHandlerSQLite::IncomingPaymentReceiptHandlerSQLite(
    sqlite3 *dbConnection,
    const string &tableName,
    Logger &logger) :

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    // Validate input parameters.
    if (dbConnection == nullptr) {
        throw ValueError("IncomingPaymentReceiptHandlerSQLite::constructor: Database connection cannot be null.");
    }

    if (tableName.empty()) {
        throw ValueError("IncomingPaymentReceiptHandlerSQLite::constructor: Table name cannot be empty.");
    }

    // Create the main table
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (trust_line_id INTEGER NOT NULL, "
                   "audit_number INTEGER NOT NULL, "
                   "transaction_uuid BLOB NOT NULL, "
                   "contractor_public_key_hash BLOB NOT NULL, "
                   "amount BLOB NOT NULL, "
                   "contractor_signature BLOB NOT NULL, "
                   "FOREIGN KEY(trust_line_id) REFERENCES trust_lines(id) ON DELETE CASCADE ON UPDATE CASCADE, "
                   "FOREIGN KEY(contractor_public_key_hash) REFERENCES contractor_keys(hash) ON DELETE CASCADE ON UPDATE CASCADE);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::constructor: Failed to create table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Create index on transaction_uuid
    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_transaction_uuid_idx on " + mTableName + "(transaction_uuid);";
    SQLiteStatementRAII transactionIndexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(transactionIndexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::constructor: Failed to create transaction_uuid index on table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "IncomingPaymentReceiptHandler initialized: table=" << mTableName;
#endif
}

void IncomingPaymentReceiptHandlerSQLite::saveRecord(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber,
    const TransactionUUID &transactionUUID,
    const KeyHash::Shared contractorPublicKeyHash,
    const TrustLineAmount &amount,
    const Signature::Shared contractorSignature)
{
    if (!contractorPublicKeyHash) {
        throw ValueError("IncomingPaymentReceiptHandlerSQLite::saveRecord: Contractor public key hash cannot be null.");
    }

    if (!contractorSignature) {
        throw ValueError("IncomingPaymentReceiptHandlerSQLite::saveRecord: Contractor signature cannot be null.");
    }

    string query = "INSERT INTO " + mTableName +
                   "(trust_line_id, audit_number, transaction_uuid, contractor_public_key_hash, "
                   "amount, contractor_signature) VALUES (?, ?, ?, ?, ?, ?);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::saveRecord: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, auditNumber);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::saveRecord: Failed to bind audit_number. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_blob(stmt.get(), 3, transactionUUID.data,
                           (int)TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::saveRecord: Failed to bind transaction_uuid. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_blob(stmt.get(), 4, contractorPublicKeyHash->data(),
                           (int)KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::saveRecord: Failed to bind contractor_public_key_hash. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    vector<byte_t> amountBufferBytes = trustLineAmountToBytes(amount);
    rc = sqlite3_bind_blob(stmt.get(), 5, amountBufferBytes.data(), kTrustLineAmountBytesCount, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::saveRecord: Failed to bind amount. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_blob(stmt.get(), 6, contractorSignature->data(),
                           (int)contractorSignature->signatureSize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::saveRecord: Failed to bind contractor_signature. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ", SignatureSize=" + to_string(contractorSignature->signatureSize()) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::saveRecord: Failed to execute INSERT. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Receipt record saved: TrustLine=" << trustLineID
           << ", AuditNumber=" << auditNumber
           << ", SignatureSize=" << contractorSignature->signatureSize();
#endif
}

map<TransactionUUID, TrustLineAmount> IncomingPaymentReceiptHandlerSQLite::auditAmounts(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber)
{
    map<TransactionUUID, TrustLineAmount> result;
    string query = "SELECT transaction_uuid, amount FROM " + mTableName + " WHERE trust_line_id = ? AND audit_number = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::auditAmounts: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, auditNumber);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::auditAmounts: Failed to bind audit_number. "
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

        result[transactionUUID] = amount;
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Audit amounts retrieved: TrustLine=" << trustLineID
           << ", AuditNumber=" << auditNumber
           << ", Count=" << result.size();
#endif

    return result;
}

vector<ReceiptRecord::Shared> IncomingPaymentReceiptHandlerSQLite::receiptsByAuditNumber(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber)
{
    vector<ReceiptRecord::Shared> result;
    string query = "SELECT amount, transaction_uuid, contractor_public_key_hash, contractor_signature FROM " + mTableName + " WHERE trust_line_id = ? AND audit_number = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::receiptsByAuditNumber: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, auditNumber);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::receiptsByAuditNumber: Failed to bind audit_number. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        auto amountBytes = (byte_t*)sqlite3_column_blob(stmt.get(), 0);
        vector<byte_t> incomingAmountBufferBytes(
            amountBytes,
            amountBytes + kTrustLineAmountBytesCount);

        TransactionUUID transactionUUID((uint8_t*)sqlite3_column_blob(stmt.get(), 1));

        auto contractorKeyHash = make_shared<KeyHash>(
                                     (byte_t*)sqlite3_column_blob(stmt.get(), 2));

        auto contractorSignature = make_shared<Signature>(
                                       (byte_t*)sqlite3_column_blob(stmt.get(), 3));

        result.push_back(make_shared<ReceiptRecord>(
                             auditNumber,
                             transactionUUID,
                             bytesToTrustLineAmount(incomingAmountBufferBytes),
                             contractorKeyHash,
                             contractorSignature));
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Receipt records retrieved: TrustLine=" << trustLineID
           << ", AuditNumber=" << auditNumber
           << ", Count=" << result.size();
#endif

    return result;
}

vector<ReceiptRecord::Shared> IncomingPaymentReceiptHandlerSQLite::receiptsLessEqualThanAuditNumber(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber)
{
    vector<ReceiptRecord::Shared> result;
    string query = "SELECT amount, transaction_uuid, contractor_public_key_hash, contractor_signature FROM " + mTableName + " WHERE trust_line_id = ? AND audit_number <= ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::receiptsLessEqualThanAuditNumber: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, auditNumber);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::receiptsLessEqualThanAuditNumber: Failed to bind audit_number. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        auto amountBytes = (byte_t*)sqlite3_column_blob(stmt.get(), 0);
        vector<byte_t> incomingAmountBufferBytes(
            amountBytes,
            amountBytes + kTrustLineAmountBytesCount);

        TransactionUUID transactionUUID((uint8_t*)sqlite3_column_blob(stmt.get(), 1));

        auto contractorKeyHash = make_shared<KeyHash>(
                                     (byte_t*)sqlite3_column_blob(stmt.get(), 2));

        auto contractorSignature = make_shared<Signature>(
                                       (byte_t*)sqlite3_column_blob(stmt.get(), 3));

        result.push_back(make_shared<ReceiptRecord>(
                             auditNumber,
                             transactionUUID,
                             bytesToTrustLineAmount(incomingAmountBufferBytes),
                             contractorKeyHash,
                             contractorSignature));
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Receipt records retrieved (<=): TrustLine=" << trustLineID
           << ", MaxAuditNumber=" << auditNumber
           << ", Count=" << result.size();
#endif

    return result;
}

vector<ReceiptRecord::Shared> IncomingPaymentReceiptHandlerSQLite::getFinalizedReceiptsWithZeroAuditNumber(
    const TrustLineID trustLineID)
{
    vector<ReceiptRecord::Shared> result;

    // Join receipts with payment transactions to filter by finalized observing state.
    string query = "SELECT r.amount, r.transaction_uuid, r.contractor_public_key_hash, r.contractor_signature "
                   "FROM " + mTableName + " r "
                   "JOIN " + string(StorageHandlerSQLite::paymentTransactionsTableName()) + " pt ON r.transaction_uuid = pt.uuid "
                   "WHERE r.trust_line_id = ? AND r.audit_number = ? AND pt.observing_state = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::getFinalizedReceiptsWithZeroAuditNumber: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, kZeroAuditNumber);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::getFinalizedReceiptsWithZeroAuditNumber: Failed to bind audit_number. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 3, kCommittedObservingState);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::getFinalizedReceiptsWithZeroAuditNumber: Failed to bind observing_state. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        auto amountBytes = (byte_t*)sqlite3_column_blob(stmt.get(), 0);
        vector<byte_t> incomingAmountBufferBytes(
            amountBytes,
            amountBytes + kTrustLineAmountBytesCount);

        TransactionUUID transactionUUID((uint8_t*)sqlite3_column_blob(stmt.get(), 1));

        auto contractorKeyHash = make_shared<KeyHash>(
                                     (byte_t*)sqlite3_column_blob(stmt.get(), 2));

        auto contractorSignature = make_shared<Signature>(
                                       (byte_t*)sqlite3_column_blob(stmt.get(), 3));

        result.push_back(make_shared<ReceiptRecord>(
                             kZeroAuditNumber,
                             transactionUUID,
                             bytesToTrustLineAmount(incomingAmountBufferBytes),
                             contractorKeyHash,
                             contractorSignature));
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Finalized receipts with zero audit number retrieved: TrustLine=" << trustLineID
           << ", Count=" << result.size();
#endif

    return result;
}

void IncomingPaymentReceiptHandlerSQLite::updateAuditNumberByTransactionUUIDs(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber,
    const vector<TransactionUUID> &transactionUUIDs)
{
    // Avoid generating invalid SQL when there are no transaction UUIDs to update.
    if (transactionUUIDs.empty()) {
        return;
    }

    string query = "UPDATE " + mTableName + " SET audit_number = ? "
                   "WHERE trust_line_id = ? AND transaction_uuid IN (";
    for (size_t i = 0; i < transactionUUIDs.size(); ++i) {
        query += "?";
        if (i + 1 < transactionUUIDs.size()) {
            query += ", ";
        }
    }
    query += ");";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, auditNumber);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::updateAuditNumberByTransactionUUIDs: Failed to bind audit_number. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::updateAuditNumberByTransactionUUIDs: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    int parameterIndex = 3;
    for (const auto &transactionUUID : transactionUUIDs) {
        rc = sqlite3_bind_blob(
            stmt.get(),
            parameterIndex,
            transactionUUID.data,
            TransactionUUID::kBytesSize,
            SQLITE_STATIC);
        if (rc != SQLITE_OK) {
            throw IOError("IncomingPaymentReceiptHandlerSQLite::updateAuditNumberByTransactionUUIDs: Failed to bind transaction_uuid. "
                          "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                          ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
        }
        ++parameterIndex;
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::updateAuditNumberByTransactionUUIDs: Failed to execute UPDATE. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

size_t IncomingPaymentReceiptHandlerSQLite::countReceiptsByNumber(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber)
{
    string query = "SELECT COUNT(transaction_uuid) FROM " + mTableName + " WHERE trust_line_id = ? AND audit_number = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::countReceiptsByNumber: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, auditNumber);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::countReceiptsByNumber: Failed to bind audit_number. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_ROW) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::countReceiptsByNumber: Failed to execute count query. "
                      "TrustLine=" + to_string(trustLineID) + ", AuditNumber=" + to_string(auditNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    auto countReceipts = (size_t)sqlite3_column_int(stmt.get(), 0);

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Receipts count retrieved: TrustLine=" << trustLineID
           << ", AuditNumber=" << auditNumber
           << ", Count=" << countReceipts;
#endif

    return countReceipts;
}

void IncomingPaymentReceiptHandlerSQLite::deleteRecords(
    const TransactionUUID &transactionUUID)
{
    string query = "DELETE FROM " + mTableName + " WHERE transaction_uuid = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, transactionUUID.data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::deleteRecords: Failed to bind transaction_uuid. "
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::deleteRecords: Failed to execute DELETE. "
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    int deletedRows = sqlite3_changes(mDataBase);

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Records deleted by transaction UUID: DeletedCount=" << deletedRows;
#endif
}

void IncomingPaymentReceiptHandlerSQLite::deleteRecords(
    const TrustLineID trustLineID)
{
    string query = "DELETE FROM " + mTableName + " WHERE trust_line_id = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::deleteRecords: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::deleteRecords: Failed to execute DELETE. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    int deletedRows = sqlite3_changes(mDataBase);

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Records deleted by trust line: TrustLine=" << trustLineID
           << ", DeletedCount=" << deletedRows;
#endif
}



bool IncomingPaymentReceiptHandlerSQLite::isContainsKeyHash(
    KeyHash::Shared keyHash)
{
    if (!keyHash) {
        throw ValueError("IncomingPaymentReceiptHandlerSQLite::isContainsKeyHash: Key hash cannot be null.");
    }

    string query = "SELECT contractor_public_key_hash FROM " + mTableName + " WHERE contractor_public_key_hash = ? LIMIT 1";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, keyHash->data(),
                               (int)KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::isContainsKeyHash: Failed to bind contractor_public_key_hash. "
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    bool result = (sqlite3_step(stmt.get()) == SQLITE_ROW);

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Key hash existence checked: Found=" << (result ? "true" : "false");
#endif

    return result;
}

bool IncomingPaymentReceiptHandlerSQLite::isContainsTransaction(
    const TransactionUUID &transactionUUID)
{
    string query = "SELECT transaction_uuid FROM " + mTableName + " WHERE transaction_uuid = ? LIMIT 1";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, transactionUUID.data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("IncomingPaymentReceiptHandlerSQLite::isContainsTransaction: Failed to bind transaction_uuid. "
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    bool result = (sqlite3_step(stmt.get()) == SQLITE_ROW);

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Transaction existence checked: Found=" << (result ? "true" : "false");
#endif

    return result;
}

LoggerStream IncomingPaymentReceiptHandlerSQLite::info() const
{
    return mLog.info(logHeader());
}

LoggerStream IncomingPaymentReceiptHandlerSQLite::warning() const
{
    return mLog.warning(logHeader());
}

const string IncomingPaymentReceiptHandlerSQLite::logHeader() const
{
    stringstream s;
    s << "[IncomingPaymentReceiptHandler]";
    return s.str();
}
