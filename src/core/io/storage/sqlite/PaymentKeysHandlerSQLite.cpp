#include "PaymentKeysHandlerSQLite.h"

PaymentKeysHandlerSQLite::PaymentKeysHandlerSQLite(
    sqlite3 *dbConnection,
    const string &tableName,
    Logger &logger) :

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    sqlite3_stmt *stmt;
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (transaction_uuid BLOB NOT NULL, "
                   "public_key BLOB NOT NULL, "
                   "private_key BLOB NOT NULL);";
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentKeysHandlerSQLite::creating table: "
                      "Bad query; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("PaymentKeysHandlerSQLite::creating table: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }

    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_transaction_uuid_idx on " + mTableName + "(transaction_uuid);";
    rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentKeysHandlerSQLite::creating  index for TransactionUUID: "
                      "Bad query; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("PaymentKeysHandlerSQLite::creating index for TransactionUUID: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }

    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
}

void PaymentKeysHandlerSQLite::saveOwnKey(
    const TransactionUUID &transactionUUID,
    const PublicKey::Shared publicKey,
    const PrivateKey *privateKey)
{
    string query = "INSERT INTO " + mTableName +
                   "(transaction_uuid, public_key, private_key) "
                   "VALUES (?, ?, ?);";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentKeysHandlerSQLite::saveOwnKey: "
                      "Bad query; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt, 1, transactionUUID.data,
                           TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentKeysHandlerSQLite::saveOwnKey: "
                      "Bad binding of TransactionUUID; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt, 2, publicKey->data(),
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
    rc = sqlite3_bind_blob(stmt, 3, buffer.get(),
                           (int)privateKey->keySize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentKeysHandlerSQLite::saveOwnKey: "
                      "Bad binding of Private Key; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
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
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentKeysHandlerSQLite::getOwnPrivateKey: "
                      "Bad query; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt, 1, transactionUUID.data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentKeysHandlerSQLite::getOwnPrivateKey: "
                      "Bad binding of TransactionUUID; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        info() << "Before private key deserializing";
        auto bytesCnt = sqlite3_column_bytes(stmt, 0);
        info() << "bytes cnt: " << bytesCnt;
        auto privateKeyBytesPtr = (byte*)sqlite3_column_blob(stmt, 0);
        info() << "privateKeyBytesPtr = " << reinterpret_cast<int64_t>(privateKeyBytesPtr);
        if (privateKeyBytesPtr == nullptr) {
            info() << "privateKeyBytesPtr is null";
        }
        auto result = new PrivateKey(reinterpret_cast<byte_t*>(privateKeyBytesPtr));
        info() << "Private key deserialized. ";
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
        return result;
    } else {
        info() << "Private key was not found";
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
        throw NotFoundError("PaymentKeysHandlerSQLite::getOwnPrivateKey: "
                            "There are now records with requested transactionUUID");
    }
}

void PaymentKeysHandlerSQLite::deleteKeyByTransactionUUID(
    const TransactionUUID &transactionUUID)
{
    string query = "DELETE FROM " + mTableName + " WHERE transaction_uuid = ?";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentKeysHandlerSQLite::deleteKeyByTransactionUUID: "
                      "Bad query; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_blob(stmt, 1, transactionUUID.data, TransactionUUID::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentKeysHandlerSQLite::deleteKeyByTransactionUUID: "
                      "Bad binding of TransactionUUID; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
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
    sqlite3_stmt *stmt;

    string query = "SELECT transaction_uuid FROM " + mTableName;
    int rc = sqlite3_prepare_v2(mDataBase, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentKeysHandlerSQLite::allTransactionUUIDs: "
                      "Bad query; sqlite error: " + to_string(rc));
    }
    while (sqlite3_step(stmt) == SQLITE_ROW ) {
        TransactionUUID transactionUUID((uint8_t*)sqlite3_column_blob(stmt, 0));

        result.emplace_back(transactionUUID);
    }
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
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
