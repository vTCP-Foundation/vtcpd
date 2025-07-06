#include "PaymentKeysHandlerSQLite.h"

PaymentKeysHandlerSQLite::PaymentKeysHandlerSQLite(
    sqlite3 *dbConnection,
    const string &tableName,
    Logger &logger) :

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (transaction_uuid BLOB NOT NULL, "
                   "public_key BLOB NOT NULL, "
                   "private_key BLOB NOT NULL);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("PaymentKeysHandlerSQLite::creating table: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }

    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_transaction_uuid_idx on " + mTableName + "(transaction_uuid);";
    SQLiteStatementRAII indexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(indexStmt.get());
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("PaymentKeysHandlerSQLite::creating index for TransactionUUID: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "PaymentKeysHandler initialized: table=" << mTableName;
#endif
}

void PaymentKeysHandlerSQLite::saveOwnKey(
    const TransactionUUID &transactionUUID,
    const PublicKey::Shared publicKey,
    const PrivateKey *privateKey)
{
    string query = "INSERT INTO " + mTableName +
                   "(transaction_uuid, public_key, private_key) "
                   "VALUES (?, ?, ?);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_blob(stmt.get(), 1, transactionUUID.data,
                               TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentKeysHandlerSQLite::saveOwnKey: "
                      "Bad binding of TransactionUUID; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt.get(), 2, publicKey->data(),
                           (int)publicKey->keySize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentKeysHandlerSQLite::saveOwnKey: "
                      "Bad binding of Public Key; sqlite error: " +
                      to_string(rc));
    }

    // todo encrypt private key data
    BytesShared buffer = tryMalloc(privateKey->keySize());
    {
        auto g = privateKey->data()->unlockAndInitGuard();
        memcpy(
            buffer.get(),
            g.address(),
            privateKey->keySize());
    }
    auto g = privateKey->data()->unlockAndInitGuard();
    rc = sqlite3_bind_blob(stmt.get(), 3, buffer.get(),
                           (int)privateKey->keySize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentKeysHandlerSQLite::saveOwnKey: "
                      "Bad binding of Private Key; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "prepare inserting is completed successfully";
#endif
    } else {
        throw IOError("PaymentKeysHandlerSQLite::saveOwnKey: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }
}

PrivateKey *PaymentKeysHandlerSQLite::getOwnPrivateKey(
    const TransactionUUID &transactionUUID)
{
    info() << "getOwnPrivateKey";
    string query = "SELECT private_key FROM " + mTableName
                   + " WHERE transaction_uuid = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_blob(stmt.get(), 1, transactionUUID.data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentKeysHandlerSQLite::getOwnPrivateKey: "
                      "Bad binding of TransactionUUID; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        info() << "Before private key deserializing";
        auto bytesCnt = sqlite3_column_bytes(stmt.get(), 0);
        info() << "bytes cnt: " << bytesCnt;
        auto privateKeyBytesPtr = (byte*)sqlite3_column_blob(stmt.get(), 0);
        info() << "privateKeyBytesPtr = " << reinterpret_cast<int64_t>(privateKeyBytesPtr);
        if (privateKeyBytesPtr == nullptr) {
            info() << "privateKeyBytesPtr is null";
        }
        auto result = new PrivateKey(reinterpret_cast<byte_t*>(privateKeyBytesPtr));
        info() << "Private key deserialized. ";
        return result;
    } else {
        info() << "Private key was not found";
        throw NotFoundError("PaymentKeysHandlerSQLite::getOwnPrivateKey: "
                            "There are now records with requested transactionUUID");
    }
}

void PaymentKeysHandlerSQLite::deleteKeyByTransactionUUID(
    const TransactionUUID &transactionUUID)
{
    string query = "DELETE FROM " + mTableName + " WHERE transaction_uuid = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_blob(stmt.get(), 1, transactionUUID.data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentKeysHandlerSQLite::deleteKeyByTransactionUUID: "
                      "Bad binding of TransactionUUID; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "deleting is completed successfully";
#endif
    } else {
        throw IOError("PaymentKeysHandlerSQLite::deleteKeyByTransactionUUID: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }
}

vector<TransactionUUID> PaymentKeysHandlerSQLite::allTransactionUUIDs()
{
    vector<TransactionUUID> result;
    string query = "SELECT transaction_uuid FROM " + mTableName;
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    while (sqlite3_step(stmt.get()) == SQLITE_ROW ) {
        TransactionUUID transactionUUID((uint8_t*)sqlite3_column_blob(stmt.get(), 0));

        result.emplace_back(transactionUUID);
    }
    return result;
}

LoggerStream PaymentKeysHandlerSQLite::info() const
{
    return mLog.info(logHeader());
}

LoggerStream PaymentKeysHandlerSQLite::warning() const
{
    return mLog.warning(logHeader());
}

const string PaymentKeysHandlerSQLite::logHeader() const
{
    stringstream s;
    s << "[PaymentKeysHandler]";
    return s.str();
}
