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
                   " (id INTEGER PRIMARY KEY AUTOINCREMENT, "
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

    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_id_idx on " + mTableName + "(id);";
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
    const PublicKey::Shared publicKey,
    const PrivateKey *privateKey)
{
    string query = "INSERT INTO " + mTableName +
                   "(public_key, private_key) "
                   "VALUES (?, ?);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_blob(stmt.get(), 1, publicKey->data(),
                           (int)publicKey->keySize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentKeysHandlerSQLite::saveOwnKey: "
                      "Bad binding of Public Key; sqlite error: " +
                      to_string(rc));
    }

    // todo encrypt private key data
    auto privateKeyData = privateKey->serialize();
    auto guard = privateKeyData.unlockAndInitGuard();
    rc = sqlite3_bind_blob(stmt.get(), 2, guard.address(),
                           (int)privateKey->privateKeySize(), SQLITE_STATIC);
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

PrivateKey *PaymentKeysHandlerSQLite::getOwnPrivateKey()
{
    info() << "getOwnPrivateKey";
    string query = "SELECT private_key FROM " + mTableName
                   + " ORDER BY id DESC LIMIT 1;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        auto bytesCnt = sqlite3_column_bytes(stmt.get(), 0);
        auto privateKeyBytesPtr = (byte*)sqlite3_column_blob(stmt.get(), 0);
        if (privateKeyBytesPtr == nullptr) {
            info() << "privateKeyBytesPtr is null";
        }
        auto result = new PrivateKey(reinterpret_cast<byte_t*>(privateKeyBytesPtr));
        return result;
    } else {
        info() << "Private key was not found";
        throw NotFoundError("PaymentKeysHandlerSQLite::getOwnPrivateKey: "
                            "There are no records in payment_keys");
    }
}

void PaymentKeysHandlerSQLite::deleteKeyByID(
    const uint64_t id)
{
    string query = "DELETE FROM " + mTableName + " WHERE id = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_int64(stmt.get(), 1, (sqlite3_int64)id);
    if (rc != SQLITE_OK) {
        throw IOError("PaymentKeysHandlerSQLite::deleteKeyByID: "
                      "Bad binding of id; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "deleting is completed successfully";
#endif
    } else {
        throw IOError("PaymentKeysHandlerSQLite::deleteKeyByID: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }
}

bool PaymentKeysHandlerSQLite::hasAnyKeys()
{
    string query = "SELECT COUNT(*) FROM " + mTableName + ";";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        return sqlite3_column_int(stmt.get(), 0) > 0;
    }
    throw IOError("PaymentKeysHandlerSQLite::hasAnyKeys: Run query; sqlite error: " + to_string(rc));
}

PublicKey::Shared PaymentKeysHandlerSQLite::getOwnPublicKey()
{
    string query = "SELECT public_key FROM " + mTableName + " ORDER BY id DESC LIMIT 1;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        auto bytesCnt = sqlite3_column_bytes(stmt.get(), 0);
        auto publicKeyBytesPtr = (byte*)sqlite3_column_blob(stmt.get(), 0);
        if (publicKeyBytesPtr == nullptr) {
            throw IOError("PaymentKeysHandlerSQLite::getOwnPublicKey: public_key is null");
        }
        auto key = make_shared<PublicKey>(reinterpret_cast<byte_t*>(publicKeyBytesPtr));
        return key;
    }
    throw NotFoundError("PaymentKeysHandlerSQLite::getOwnPublicKey: no public key found");
}

uint64_t PaymentKeysHandlerSQLite::latestKeyID()
{
    string query = "SELECT id FROM " + mTableName + " ORDER BY id DESC LIMIT 1;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        return static_cast<uint64_t>(sqlite3_column_int64(stmt.get(), 0));
    }
    throw NotFoundError("PaymentKeysHandlerSQLite::latestKeyID: no keys found");
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
