#include "ContractorKeysHandlerSQLite.h"

ContractorKeysHandlerSQLite::ContractorKeysHandlerSQLite(
    sqlite3 *dbConnection,
    const string &tableName,
    Logger &logger) :

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    // Validate input parameters.
    if (dbConnection == nullptr) {
        throw ValueError("ContractorKeysHandlerSQLite::constructor: Database connection cannot be null.");
    }

    if (tableName.empty()) {
        throw ValueError("ContractorKeysHandlerSQLite::constructor: Table name cannot be empty.");
    }

    // Create the main table
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (hash BLOB PRIMARY KEY, "
                   "trust_line_id INTEGER NOT NULL, "
                   "keys_set_sequence_number INTEGER NOT NULL, "
                   "public_key BLOB NOT NULL, "
                   "number INTEGER NOT NULL, "
                   "is_valid INTEGER NOT NULL DEFAULT 1, "
                   "FOREIGN KEY(trust_line_id) REFERENCES trust_lines(id) ON DELETE CASCADE ON UPDATE CASCADE);";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("ContractorKeysHandlerSQLite::constructor: Failed to create table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Create unique index on hash
    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName + "_hash_idx on " + mTableName + "(hash);";
    SQLiteStatementRAII hashIndexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(hashIndexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("ContractorKeysHandlerSQLite::constructor: Failed to create hash index on table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Create index on trust_line_id
    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_trust_line_id_idx on " + mTableName + "(trust_line_id);";
    SQLiteStatementRAII trustLineIndexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(trustLineIndexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("ContractorKeysHandlerSQLite::constructor: Failed to create trust_line_id index on table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "ContractorKeysHandler initialized: table=" << mTableName;
#endif
}

void ContractorKeysHandlerSQLite::saveKey(
    const TrustLineID trustLineID,
    const KeyNumber keysSetSequenceNumber,
    const PublicKey::Shared publicKey,
    const KeyNumber number)
{
    if (!publicKey) {
        throw ValueError("ContractorKeysHandlerSQLite::saveKey: Public key cannot be null.");
    }

    string query = "INSERT INTO " + mTableName +
                   "(hash, trust_line_id, keys_set_sequence_number, public_key, "
                   "number) VALUES (?, ?, ?, ?, ?);";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    // Store the result of publicKey->hash() in a local shared_ptr (keyHashShared).
    // This ensures that the underlying KeyHash object and its data remain valid
    // for the duration of the sqlite3_bind_blob call.
    auto keyHashShared = publicKey->hash();

    // Bind parameters
    int rc = sqlite3_bind_blob(stmt.get(), 1, keyHashShared->data(),
                               (int)KeyHash::kBytesSize, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::saveKey: Failed to bind hash. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::saveKey: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 3, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::saveKey: Failed to bind sequence_number. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_blob(stmt.get(), 4, publicKey->data(), (int)publicKey->keySize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::saveKey: Failed to bind public_key. "
                      "TrustLine=" + to_string(trustLineID) + ", KeySize=" + to_string(publicKey->keySize()) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 5, number);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::saveKey: Failed to bind number. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("ContractorKeysHandlerSQLite::saveKey: Failed to execute INSERT. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Key saved: TrustLine=" << trustLineID
           << ", KeyNumber=" << number
           << ", Sequence=" << keysSetSequenceNumber
           << ", KeySize=" << publicKey->keySize();
#endif
}

const KeyNumber ContractorKeysHandlerSQLite::maxKeySetSequenceNumber(
    const TrustLineID trustLineID)
{
    string query = "SELECT MAX(keys_set_sequence_number) FROM " + mTableName + " WHERE trust_line_id = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::maxKeySetSequenceNumber: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        // Check if result is NULL
        if (sqlite3_column_type(stmt.get(), 0) == SQLITE_NULL) {
            throw NotFoundError("ContractorKeysHandlerSQLite::maxKeySetSequenceNumber: No keys found. "
                                "TrustLine=" + to_string(trustLineID) + ".");
        }
        KeyNumber maxSequence = (KeyNumber)sqlite3_column_int(stmt.get(), 0);

#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Max sequence retrieved: TrustLine=" << trustLineID
               << ", MaxSequence=" << maxSequence;
#endif
        return maxSequence;
    } else {
        throw IOError("ContractorKeysHandlerSQLite::maxKeySetSequenceNumber: Failed to execute SELECT. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

void ContractorKeysHandlerSQLite::invalidKey(
    const TrustLineID trustLineID,
    const KeyNumber number)
{
    string query = "UPDATE " + mTableName + " SET is_valid = 0 WHERE trust_line_id = ? AND number = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::invalidKey: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, number);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::invalidKey: Failed to bind number. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("ContractorKeysHandlerSQLite::invalidKey: Failed to execute UPDATE. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    if (sqlite3_changes(mDataBase) == 0) {
        throw ValueError("ContractorKeysHandlerSQLite::invalidKey: No rows affected. "
                         "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) + ".");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Key invalidated: TrustLine=" << trustLineID
           << ", KeyNumber=" << number;
#endif
}

void ContractorKeysHandlerSQLite::invalidateKeyByHash(
    const TrustLineID trustLineID,
    const KeyHash::Shared keyHash)
{
    if (!keyHash) {
        throw ValueError("ContractorKeysHandlerSQLite::invalidateKeyByHash: Key hash cannot be null.");
    }

    string query = "UPDATE " + mTableName + " SET is_valid = 0 WHERE trust_line_id = ? AND hash = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::invalidateKeyByHash: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_blob(stmt.get(), 2, keyHash->data(),
                           (int)KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::invalidateKeyByHash: Failed to bind hash. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("ContractorKeysHandlerSQLite::invalidateKeyByHash: Failed to execute UPDATE. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    if (sqlite3_changes(mDataBase) == 0) {
        throw ValueError("ContractorKeysHandlerSQLite::invalidateKeyByHash: No rows affected. "
                         "TrustLine=" + to_string(trustLineID) + ".");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Key invalidated by hash: TrustLine=" << trustLineID;
#endif
}

PublicKey::Shared ContractorKeysHandlerSQLite::keyByNumber(
    const TrustLineID trustLineID,
    const KeyNumber keyNumber)
{
    string query = "SELECT public_key FROM " + mTableName + " WHERE trust_line_id = ? AND number = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::keyByNumber: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(keyNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, keyNumber);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::keyByNumber: Failed to bind number. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(keyNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        auto publicKey = make_shared<PublicKey>((byte_t*)sqlite3_column_blob(stmt.get(), 0));

#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Public key retrieved by number: TrustLine=" << trustLineID
               << ", KeyNumber=" << keyNumber;
#endif
        return publicKey;
    } else if (rc == SQLITE_DONE) {
        throw NotFoundError("ContractorKeysHandlerSQLite::keyByNumber: Public key not found. "
                            "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(keyNumber) + ".");
    } else {
        throw IOError("ContractorKeysHandlerSQLite::keyByNumber: Failed to execute SELECT. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(keyNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

PublicKey::Shared ContractorKeysHandlerSQLite::keyByHash(
    const TrustLineID trustLineID,
    const KeyHash::Shared keyHash)
{
    if (!keyHash) {
        throw ValueError("ContractorKeysHandlerSQLite::keyByHash: Key hash cannot be null.");
    }

    string query = "SELECT public_key FROM " + mTableName + " WHERE trust_line_id = ? AND hash = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::keyByHash: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_blob(stmt.get(), 2, keyHash->data(),
                           (int)KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::keyByHash: Failed to bind hash. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        auto publicKey = make_shared<PublicKey>((byte_t*)sqlite3_column_blob(stmt.get(), 0));

#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Public key retrieved by hash: TrustLine=" << trustLineID;
#endif
        return publicKey;
    } else if (rc == SQLITE_DONE) {
        throw NotFoundError("ContractorKeysHandlerSQLite::keyByHash: Public key not found. "
                            "TrustLine=" + to_string(trustLineID) + ".");
    } else {
        throw IOError("ContractorKeysHandlerSQLite::keyByHash: Failed to execute SELECT. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

const KeyHash::Shared ContractorKeysHandlerSQLite::keyHashByNumber(
    const TrustLineID trustLineID,
    const KeyNumber keyNumber)
{
    string query = "SELECT hash FROM " + mTableName + " WHERE trust_line_id = ? AND number = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::keyHashByNumber: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(keyNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, keyNumber);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::keyHashByNumber: Failed to bind number. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(keyNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        auto hash = make_shared<KeyHash>((byte_t*)sqlite3_column_blob(stmt.get(), 0));

#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Key hash retrieved by number: TrustLine=" << trustLineID
               << ", KeyNumber=" << keyNumber;
#endif
        return hash;
    } else if (rc == SQLITE_DONE) {
        throw NotFoundError("ContractorKeysHandlerSQLite::keyHashByNumber: Key hash not found. "
                            "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(keyNumber) + ".");
    } else {
        throw IOError("ContractorKeysHandlerSQLite::keyHashByNumber: Failed to execute SELECT. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(keyNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

KeysCount ContractorKeysHandlerSQLite::availableKeysCnt(
    const TrustLineID trustLineID)
{
    string query = "SELECT count(*) FROM " + mTableName +
                   " WHERE trust_line_id = ? AND is_valid = 1";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::availableKeysCnt: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        KeysCount count = (KeysCount)sqlite3_column_int(stmt.get(), 0);

#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Available keys count: TrustLine=" << trustLineID
               << ", Count=" << count;
#endif
        return count;
    } else {
        throw IOError("ContractorKeysHandlerSQLite::availableKeysCnt: Failed to execute count query. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

KeysCount ContractorKeysHandlerSQLite::sequenceKeysCnt(
    const TrustLineID trustLineID,
    KeyNumber keysSetSequenceNumber)
{
    string query = "SELECT count(*) FROM " + mTableName +
                   " WHERE trust_line_id = ? AND keys_set_sequence_number = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::sequenceKeysCnt: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::sequenceKeysCnt: Failed to bind sequence_number. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        KeysCount count = (KeysCount)sqlite3_column_int(stmt.get(), 0);

#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Sequence keys count: TrustLine=" << trustLineID
               << ", Sequence=" << keysSetSequenceNumber
               << ", Count=" << count;
#endif
        return count;
    } else {
        throw IOError("ContractorKeysHandlerSQLite::sequenceKeysCnt: Failed to execute count query. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

void ContractorKeysHandlerSQLite::removeUnusedKeys(
    const TrustLineID trustLineID)
{
    string query = "DELETE FROM " + mTableName + " WHERE trust_line_id = ? AND is_valid = 1";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::removeUnusedKeys: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("ContractorKeysHandlerSQLite::removeUnusedKeys: Failed to execute DELETE. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    int deletedRows = sqlite3_changes(mDataBase);

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Unused keys removed: TrustLine=" << trustLineID
           << ", DeletedCount=" << deletedRows;
#endif
}

vector<PublicKey::Shared> ContractorKeysHandlerSQLite::publicKeysBySetNumber(
    const TrustLineID trustLineID,
    const KeyNumber keysSetSequenceNumber) const
{
    vector<PublicKey::Shared> result;

    // First, get the count to reserve space
    string countQuery = "SELECT count(*) FROM " + mTableName +
                        " WHERE trust_line_id = ? AND keys_set_sequence_number = ?";
    SQLiteStatementRAII countStmt(mDataBase, countQuery.c_str());

    int rc = sqlite3_bind_int(countStmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::publicKeysBySetNumber: Failed to bind trust_line_id in count query. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(countStmt.get(), 2, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::publicKeysBySetNumber: Failed to bind sequence_number in count query. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(countStmt.get());
    if (rc == SQLITE_ROW) {
        auto rowCount = (uint32_t)sqlite3_column_int(countStmt.get(), 0);
        result.reserve(rowCount);

#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Retrieving public keys by sequence: TrustLine=" << trustLineID
               << ", Sequence=" << keysSetSequenceNumber
               << ", ExpectedCount=" << rowCount;
#endif
    }

    // Now get the actual data
    string query = "SELECT public_key FROM " + mTableName +
                   " WHERE trust_line_id = ? AND keys_set_sequence_number = ? ORDER BY number";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::publicKeysBySetNumber: Failed to bind trust_line_id in data query. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::publicKeysBySetNumber: Failed to bind sequence_number in data query. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        auto publicKey = make_shared<PublicKey>((byte_t*)sqlite3_column_blob(stmt.get(), 0));
        result.push_back(publicKey);
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Public keys retrieved: TrustLine=" << trustLineID
           << ", Sequence=" << keysSetSequenceNumber
           << ", ActualCount=" << result.size();
#endif

    return result;
}

void ContractorKeysHandlerSQLite::deleteKeysByTrustLineID(
    const TrustLineID trustLineID)
{
    string query = "DELETE FROM " + mTableName + " WHERE trust_line_id = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::deleteKeysByTrustLineID: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("ContractorKeysHandlerSQLite::deleteKeysByTrustLineID: Failed to execute DELETE. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    int deletedRows = sqlite3_changes(mDataBase);

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "All keys deleted: TrustLine=" << trustLineID
           << ", DeletedCount=" << deletedRows;
#endif
}

void ContractorKeysHandlerSQLite::deleteKeyByHashExceptSequenceNumber(
    KeyHash::Shared keyHash,
    const KeyNumber keysSetSequenceNumber)
{
    if (!keyHash) {
        throw ValueError("ContractorKeysHandlerSQLite::deleteKeyByHashExceptSequenceNumber: Key hash cannot be null.");
    }

    string query = "DELETE FROM " + mTableName +
                   " WHERE hash = ? AND keys_set_sequence_number != ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, keyHash->data(),
                               (int)KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::deleteKeyByHashExceptSequenceNumber: Failed to bind hash. "
                      "Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::deleteKeyByHashExceptSequenceNumber: Failed to bind sequence_number. "
                      "Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("ContractorKeysHandlerSQLite::deleteKeyByHashExceptSequenceNumber: Failed to execute DELETE. "
                      "Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    int deletedRows = sqlite3_changes(mDataBase);

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Keys deleted by hash: PreservedSequence=" << keysSetSequenceNumber
           << ", DeletedCount=" << deletedRows;
#endif
}

vector<KeyHash::Shared> ContractorKeysHandlerSQLite::publicKeyHashesLessThanSetNumber(
    const TrustLineID trustLineID,
    const KeyNumber keysSetSequenceNumber) const
{
    vector<KeyHash::Shared> result;

    // First, get the count to reserve space
    string countQuery = "SELECT count(*) FROM " + mTableName +
                        " WHERE trust_line_id = ? AND keys_set_sequence_number < ?";
    SQLiteStatementRAII countStmt(mDataBase, countQuery.c_str());

    int rc = sqlite3_bind_int(countStmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::publicKeyHashesLessThanSetNumber: Failed to bind trust_line_id in count query. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(countStmt.get(), 2, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::publicKeyHashesLessThanSetNumber: Failed to bind sequence_number in count query. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(countStmt.get());
    if (rc == SQLITE_ROW) {
        auto rowCount = (uint32_t)sqlite3_column_int(countStmt.get(), 0);
        result.reserve(rowCount);

#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Retrieving key hashes less than sequence: TrustLine=" << trustLineID
               << ", Sequence=" << keysSetSequenceNumber
               << ", ExpectedCount=" << rowCount;
#endif
    }

    // Now get the actual data
    string query = "SELECT hash FROM " + mTableName +
                   " WHERE trust_line_id = ? AND keys_set_sequence_number < ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::publicKeyHashesLessThanSetNumber: Failed to bind trust_line_id in data query. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandlerSQLite::publicKeyHashesLessThanSetNumber: Failed to bind sequence_number in data query. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        auto hash = make_shared<KeyHash>((byte_t*)sqlite3_column_blob(stmt.get(), 0));
        result.push_back(hash);
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Key hashes retrieved: TrustLine=" << trustLineID
           << ", Sequence=" << keysSetSequenceNumber
           << ", ActualCount=" << result.size();
#endif

    return result;
}

LoggerStream ContractorKeysHandlerSQLite::info() const
{
    return mLog.info(logHeader());
}

LoggerStream ContractorKeysHandlerSQLite::warning() const
{
    return mLog.warning(logHeader());
}

const string ContractorKeysHandlerSQLite::logHeader() const
{
    stringstream s;
    s << "[ContractorKeysHandler: (" << mTableName << ")]";
    return s.str();
}
