#include "ContractorKeysHandler.h"

ContractorKeysHandler::ContractorKeysHandler(
    sqlite3 *dbConnection,
    const string &tableName,
    Logger &logger) :

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    // Validate input parameters.
    if (dbConnection == nullptr) {
        throw ValueError("ContractorKeysHandler::constructor: Database connection cannot be null.");
    }

    if (tableName.empty()) {
        throw ValueError("ContractorKeysHandler::constructor: Table name cannot be empty.");
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
        throw IOError("ContractorKeysHandler::constructor: Failed to create table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Create unique index on hash
    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName + "_hash_idx on " + mTableName + "(hash);";
    SQLiteStatementRAII hashIndexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(hashIndexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("ContractorKeysHandler::constructor: Failed to create hash index on table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Create index on trust_line_id
    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_trust_line_id_idx on " + mTableName + "(trust_line_id);";
    SQLiteStatementRAII trustLineIndexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(trustLineIndexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("ContractorKeysHandler::constructor: Failed to create trust_line_id index on table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "ContractorKeysHandler initialized: table=" << mTableName;
#endif
}

void ContractorKeysHandler::saveKey(
    const TrustLineID trustLineID,
    const KeyNumber keysSetSequenceNumber,
    const PublicKey::Shared publicKey,
    const KeyNumber number)
{
    if (!publicKey) {
        throw ValueError("ContractorKeysHandler::saveKey: Public key cannot be null.");
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
        throw IOError("ContractorKeysHandler::saveKey: Failed to bind hash. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::saveKey: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 3, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::saveKey: Failed to bind sequence_number. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_blob(stmt.get(), 4, publicKey->data(), (int)publicKey->keySize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::saveKey: Failed to bind public_key. "
                      "TrustLine=" + to_string(trustLineID) + ", KeySize=" + to_string(publicKey->keySize()) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 5, number);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::saveKey: Failed to bind number. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("ContractorKeysHandler::saveKey: Failed to execute INSERT. "
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

const KeyNumber ContractorKeysHandler::maxKeySetSequenceNumber(
    const TrustLineID trustLineID)
{
    string query = "SELECT MAX(keys_set_sequence_number) FROM " + mTableName + " WHERE trust_line_id = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::maxKeySetSequenceNumber: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        // Check if result is NULL
        if (sqlite3_column_type(stmt.get(), 0) == SQLITE_NULL) {
            throw NotFoundError("ContractorKeysHandler::maxKeySetSequenceNumber: No keys found. "
                                "TrustLine=" + to_string(trustLineID) + ".");
        }
        KeyNumber maxSequence = (KeyNumber)sqlite3_column_int(stmt.get(), 0);

#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Max sequence retrieved: TrustLine=" << trustLineID
               << ", MaxSequence=" << maxSequence;
#endif
        return maxSequence;
    } else {
        throw IOError("ContractorKeysHandler::maxKeySetSequenceNumber: Failed to execute SELECT. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

void ContractorKeysHandler::invalidKey(
    const TrustLineID trustLineID,
    const KeyNumber number)
{
    string query = "UPDATE " + mTableName + " SET is_valid = 0 WHERE trust_line_id = ? AND number = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::invalidKey: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, number);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::invalidKey: Failed to bind number. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("ContractorKeysHandler::invalidKey: Failed to execute UPDATE. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    if (sqlite3_changes(mDataBase) == 0) {
        throw ValueError("ContractorKeysHandler::invalidKey: No rows affected. "
                         "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) + ".");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Key invalidated: TrustLine=" << trustLineID
           << ", KeyNumber=" << number;
#endif
}

void ContractorKeysHandler::invalidateKeyByHash(
    const TrustLineID trustLineID,
    const KeyHash::Shared keyHash)
{
    if (!keyHash) {
        throw ValueError("ContractorKeysHandler::invalidateKeyByHash: Key hash cannot be null.");
    }

    string query = "UPDATE " + mTableName + " SET is_valid = 0 WHERE trust_line_id = ? AND hash = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::invalidateKeyByHash: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_blob(stmt.get(), 2, keyHash->data(),
                           (int)KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::invalidateKeyByHash: Failed to bind hash. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("ContractorKeysHandler::invalidateKeyByHash: Failed to execute UPDATE. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    if (sqlite3_changes(mDataBase) == 0) {
        throw ValueError("ContractorKeysHandler::invalidateKeyByHash: No rows affected. "
                         "TrustLine=" + to_string(trustLineID) + ".");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Key invalidated by hash: TrustLine=" << trustLineID;
#endif
}

PublicKey::Shared ContractorKeysHandler::keyByNumber(
    const TrustLineID trustLineID,
    const KeyNumber number)
{
    string query = "SELECT public_key FROM " + mTableName +
                   " WHERE trust_line_id = ? AND number = ? AND is_valid = 1;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::keyByNumber: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, number);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::keyByNumber: Failed to bind number. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        auto result = make_shared<PublicKey>((byte_t*)sqlite3_column_blob(stmt.get(), 0));
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Public key retrieved: TrustLine=" << trustLineID
               << ", KeyNumber=" << number;
#endif
        return result;
    } else if (rc == SQLITE_DONE) {
        throw NotFoundError("ContractorKeysHandler::keyByNumber: Key not found. "
                            "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) + ".");
    } else {
        throw IOError("ContractorKeysHandler::keyByNumber: Failed to execute SELECT. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

PublicKey::Shared ContractorKeysHandler::keyByHash(
    const TrustLineID trustLineID,
    const KeyHash::Shared keyHash)
{
    if (!keyHash) {
        throw ValueError("ContractorKeysHandler::keyByHash: Key hash cannot be null.");
    }

    string query = "SELECT public_key FROM " + mTableName +
                   " WHERE trust_line_id = ? AND hash = ? AND is_valid = 1;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::keyByHash: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_blob(stmt.get(), 2, keyHash->data(),
                           (int)KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::keyByHash: Failed to bind hash. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        auto result = make_shared<PublicKey>((byte_t*)sqlite3_column_blob(stmt.get(), 0));
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Public key retrieved by hash: TrustLine=" << trustLineID;
#endif
        return result;
    } else if (rc == SQLITE_DONE) {
        throw NotFoundError("ContractorKeysHandler::keyByHash: Key not found. "
                            "TrustLine=" + to_string(trustLineID) + ".");
    } else {
        throw IOError("ContractorKeysHandler::keyByHash: Failed to execute SELECT. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

const KeyHash::Shared ContractorKeysHandler::keyHashByNumber(
    const TrustLineID trustLineID,
    const KeyNumber number)
{
    string query = "SELECT hash FROM " + mTableName +
                   " WHERE trust_line_id = ? AND number = ? AND is_valid = 1;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::keyHashByNumber: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, number);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::keyHashByNumber: Failed to bind number. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        auto result = make_shared<KeyHash>((byte_t*)sqlite3_column_blob(stmt.get(), 0));
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Key hash retrieved: TrustLine=" << trustLineID
               << ", KeyNumber=" << number;
#endif
        return result;
    } else if (rc == SQLITE_DONE) {
        throw NotFoundError("ContractorKeysHandler::keyHashByNumber: Key hash not found. "
                            "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) + ".");
    } else {
        throw IOError("ContractorKeysHandler::keyHashByNumber: Failed to execute SELECT. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

KeysCount ContractorKeysHandler::availableKeysCnt(
    const TrustLineID trustLineID)
{
    string query = "SELECT count(*) FROM " + mTableName +
                   " WHERE trust_line_id = ? AND is_valid = 1";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::availableKeysCnt: Failed to bind trust_line_id. "
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
        throw IOError("ContractorKeysHandler::availableKeysCnt: Failed to execute count query. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

KeysCount ContractorKeysHandler::sequenceKeysCnt(
    const TrustLineID trustLineID,
    KeyNumber keysSetSequenceNumber)
{
    string query = "SELECT count(*) FROM " + mTableName +
                   " WHERE trust_line_id = ? AND keys_set_sequence_number = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::sequenceKeysCnt: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::sequenceKeysCnt: Failed to bind sequence_number. "
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
        throw IOError("ContractorKeysHandler::sequenceKeysCnt: Failed to execute count query. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

void ContractorKeysHandler::removeUnusedKeys(
    const TrustLineID trustLineID)
{
    string query = "DELETE FROM " + mTableName + " WHERE trust_line_id = ? AND is_valid = 1";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::removeUnusedKeys: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("ContractorKeysHandler::removeUnusedKeys: Failed to execute DELETE. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    int deletedRows = sqlite3_changes(mDataBase);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Unused keys removed: TrustLine=" << trustLineID
           << ", DeletedCount=" << deletedRows;
#endif
}

vector<PublicKey::Shared> ContractorKeysHandler::publicKeysBySetNumber(
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
        throw IOError("ContractorKeysHandler::publicKeysBySetNumber: Failed to bind trust_line_id in count query. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(countStmt.get(), 2, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::publicKeysBySetNumber: Failed to bind sequence_number in count query. "
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
        throw IOError("ContractorKeysHandler::publicKeysBySetNumber: Failed to bind trust_line_id in data query. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::publicKeysBySetNumber: Failed to bind sequence_number in data query. "
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

void ContractorKeysHandler::deleteKeysByTrustLineID(
    const TrustLineID trustLineID)
{
    string query = "DELETE FROM " + mTableName + " WHERE trust_line_id = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::deleteKeysByTrustLineID: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("ContractorKeysHandler::deleteKeysByTrustLineID: Failed to execute DELETE. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    int deletedRows = sqlite3_changes(mDataBase);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "All keys deleted: TrustLine=" << trustLineID
           << ", DeletedCount=" << deletedRows;
#endif
}

void ContractorKeysHandler::deleteKeyByHashExceptSequenceNumber(
    KeyHash::Shared keyHash,
    const KeyNumber keysSetSequenceNumber)
{
    if (!keyHash) {
        throw ValueError("ContractorKeysHandler::deleteKeyByHashExceptSequenceNumber: Key hash cannot be null.");
    }

    string query = "DELETE FROM " + mTableName +
                   " WHERE hash = ? AND keys_set_sequence_number != ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, keyHash->data(),
                               (int)KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::deleteKeyByHashExceptSequenceNumber: Failed to bind hash. "
                      "Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::deleteKeyByHashExceptSequenceNumber: Failed to bind sequence_number. "
                      "Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("ContractorKeysHandler::deleteKeyByHashExceptSequenceNumber: Failed to execute DELETE. "
                      "Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    int deletedRows = sqlite3_changes(mDataBase);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Keys deleted by hash: PreservedSequence=" << keysSetSequenceNumber
           << ", DeletedCount=" << deletedRows;
#endif
}

vector<KeyHash::Shared> ContractorKeysHandler::publicKeyHashesLessThanSetNumber(
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
        throw IOError("ContractorKeysHandler::publicKeyHashesLessThanSetNumber: Failed to bind trust_line_id in count query. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(countStmt.get(), 2, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::publicKeyHashesLessThanSetNumber: Failed to bind sequence_number in count query. "
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
        throw IOError("ContractorKeysHandler::publicKeyHashesLessThanSetNumber: Failed to bind trust_line_id in data query. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("ContractorKeysHandler::publicKeyHashesLessThanSetNumber: Failed to bind sequence_number in data query. "
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

LoggerStream ContractorKeysHandler::info() const
{
    return mLog.info(logHeader());
}

LoggerStream ContractorKeysHandler::warning() const
{
    return mLog.warning(logHeader());
}

const string ContractorKeysHandler::logHeader() const
{
    stringstream s;
    s << "[ContractorKeysHandler: (" << mTableName << ")]";
    return s.str();
}
