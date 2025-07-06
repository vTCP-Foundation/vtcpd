#include "PaymentParticipantsVotesHandlerSQLite.h"

PaymentParticipantsVotesHandlerSQLite::PaymentParticipantsVotesHandlerSQLite(
    sqlite3 *dbConnection,
    const string &tableName,
    Logger &logger) :

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (transaction_uuid BLOB NOT NULL, "
                   "contractor BLOB NOT NULL, "
                   "payment_node_id INTEGER NOT NULL, "
                   "public_key BLOB NOT NULL, "
                   "signature BLOB NOT NULL); ";
    //"FOREIGN KEY(transaction_uuid) REFERENCES payment_transactions(uuid) ON DELETE CASCADE ON UPDATE CASCADE);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("PaymentParticipantsVotesHandlerSQLite::creating table: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }
    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_transaction_uuid_idx on " + mTableName + " (transaction_uuid);";
    SQLiteStatementRAII stmtIdx(mDataBase, query.c_str());
    rc = sqlite3_step(stmtIdx.get());
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("PaymentParticipantsVotesHandlerSQLite::creating index for TransactionUUID: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "PaymentParticipantsVotesHandler initialized: table=" << mTableName;
#endif
}

void PaymentParticipantsVotesHandlerSQLite::saveRecord(
    const TransactionUUID &transactionUUID,
    Contractor::Shared contractor,
    const PaymentNodeID paymentNodeID,
    const PublicKey::Shared publicKey,
    const Signature::Shared signature)
{
    string query = "INSERT INTO " + mTableName + " (transaction_uuid, contractor, "
                   "payment_node_id, public_key, signature) VALUES(?, ?, ?, ?, ?);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_blob(stmt.get(), 1, transactionUUID.data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentParticipantsVotesHandlerSQLite::saveRecord: "
                      "Bad binding of TransactionUUID; sqlite error: " +
                      to_string(rc));
    }
    auto contractorSerializedData = contractor->serializeToBytes();
    rc = sqlite3_bind_blob(stmt.get(), 2, contractorSerializedData.get(), (int)contractor->serializedSize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentParticipantsVotesHandlerSQLite::saveRecord: "
                      "Bad binding of Contractor; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 3, paymentNodeID);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentParticipantsVotesHandlerSQLite::saveRecord: "
                      "Bad binding of PaymentNodeID; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt.get(), 4, publicKey->data(), (int)publicKey->keySize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentParticipantsVotesHandlerSQLite::saveRecord: "
                      "Bad binding of Public Key; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt.get(), 5, signature->data(), (int)signature->signatureSize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentParticipantsVotesHandlerSQLite::saveRecord: "
                      "Bad binding of Signature; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "prepare inserting is completed successfully";
#endif
    } else {
        throw IOError("PaymentParticipantsVotesHandlerSQLite::saveRecord: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }
}

map<PaymentNodeID, Signature::Shared> PaymentParticipantsVotesHandlerSQLite::participantsSignatures(
    const TransactionUUID &transactionUUID)
{
    string query = "SELECT payment_node_id, signature FROM " + mTableName + " WHERE transaction_uuid = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_blob(stmt.get(), 1, transactionUUID.data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentParticipantsVotesHandlerSQLite::participantsSignatures: "
                      "Bad binding of TransactionUUID; sqlite error: " +
                      to_string(rc));
    }
    map<PaymentNodeID, Signature::Shared> result;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        auto paymentNodeID = (PaymentNodeID)sqlite3_column_int(stmt.get(), 0);
        auto signature = make_shared<Signature>(
                             (byte_t*)sqlite3_column_blob(stmt.get(), 1));
        result.insert(
            make_pair(
                paymentNodeID,
                signature));
    }
    return result;
}

void PaymentParticipantsVotesHandlerSQLite::deleteRecords(
    const TransactionUUID &transactionUUID)
{
    string query = "DELETE FROM " + mTableName + " WHERE transaction_uuid = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_blob(stmt.get(), 1, transactionUUID.data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentParticipantsVotesHandlerSQLite::deleteRecords: "
                      "Bad binding of TransactionUUID; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "deleting is completed successfully";
#endif
    } else {
        throw IOError("PaymentParticipantsVotesHandlerSQLite::deleteRecords: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }
}

LoggerStream PaymentParticipantsVotesHandlerSQLite::info() const
{
    return mLog.info(logHeader());
}

LoggerStream PaymentParticipantsVotesHandlerSQLite::warning() const
{
    return mLog.warning(logHeader());
}

const string PaymentParticipantsVotesHandlerSQLite::logHeader() const
{
    stringstream s;
    s << "[PaymentParticipantsVotesHandler]";
    return s.str();
}