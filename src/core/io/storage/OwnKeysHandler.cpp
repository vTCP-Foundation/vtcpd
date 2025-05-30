#include "OwnKeysHandler.h"
#include "SQLiteStatementRAII.h"

OwnKeysHandler::OwnKeysHandler(
    sqlite3 *dbConnection,
    const string &tableName,
    Logger &logger) :

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    // Validate input parameters
    if (dbConnection == nullptr) {
        throw ValueError("OwnKeysHandler::constructor: Database connection cannot be null.");
    }

    if (tableName.empty()) {
        throw ValueError("OwnKeysHandler::constructor: Table name cannot be empty.");
    }

    // Create the main table
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (hash BLOB PRIMARY KEY, "
                   "trust_line_id INTEGER NOT NULL, "
                   "keys_set_sequence_number INTEGER NOT NULL, "
                   "public_key BLOB NOT NULL, "
                   "private_key BLOB NOT NULL, "
                   "number INTEGER NOT NULL, "
                   "is_valid INTEGER NOT NULL DEFAULT 1, "
                   "FOREIGN KEY(trust_line_id) REFERENCES trust_lines(id) ON DELETE CASCADE ON UPDATE CASCADE);";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("OwnKeysHandler::constructor: Failed to create table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Create unique index on hash
    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName + "_hash_idx on " + mTableName + "(hash);";
    SQLiteStatementRAII hashIndexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(hashIndexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("OwnKeysHandler::constructor: Failed to create hash index on table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Create index on trust_line_id
    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_trust_line_id_idx on " + mTableName + "(trust_line_id);";
    SQLiteStatementRAII trustLineIndexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(trustLineIndexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("OwnKeysHandler::constructor: Failed to create trust_line_id index on table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "OwnKeysHandler initialized: table=" << mTableName;
#endif
}

void OwnKeysHandler::saveKey(
    const TrustLineID trustLineID,
    const KeyNumber keysSetSequenceNumber,
    const PublicKey::Shared publicKey,
    const PrivateKey *privateKey,
    const KeyNumber number)
{
    if (!publicKey) {
        throw ValueError("OwnKeysHandler::saveKey: Public key cannot be null.");
    }

    if (privateKey == nullptr) {
        throw ValueError("OwnKeysHandler::saveKey: Private key cannot be null.");
    }

    string query = "INSERT INTO " + mTableName +
                   "(hash, trust_line_id, keys_set_sequence_number, public_key, private_key, number) "
                   "VALUES (?, ?, ?, ?, ?, ?);";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    // Store the result of publicKey->hash() in a local shared_ptr to ensure data validity
    auto keyHashShared = publicKey->hash();

    // Bind hash
    int rc = sqlite3_bind_blob(stmt.get(), 1, keyHashShared->data(),
                               (int)KeyHash::kBytesSize, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::saveKey: Failed to bind hash. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Bind trust_line_id
    rc = sqlite3_bind_int(stmt.get(), 2, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::saveKey: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Bind keys_set_sequence_number
    rc = sqlite3_bind_int(stmt.get(), 3, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::saveKey: Failed to bind sequence_number. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Bind public_key
    rc = sqlite3_bind_blob(stmt.get(), 4, publicKey->data(),
                           (int)publicKey->keySize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::saveKey: Failed to bind public_key. "
                      "TrustLine=" + to_string(trustLineID) + ", KeySize=" + to_string(publicKey->keySize()) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Bind private_key - securely copy private key data
    BytesShared buffer = tryMalloc(PrivateKey::keySize());
    {
        auto guard = privateKey->data()->unlockAndInitGuard();
        memcpy(buffer.get(), guard.address(), PrivateKey::keySize());
    }

    rc = sqlite3_bind_blob(stmt.get(), 5, buffer.get(),
                           (int)PrivateKey::keySize(), SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::saveKey: Failed to bind private_key. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Bind number
    rc = sqlite3_bind_int(stmt.get(), 6, number);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::saveKey: Failed to bind number. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("OwnKeysHandler::saveKey: Failed to execute INSERT. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Key saved: TrustLine=" << trustLineID
           << ", KeyNumber=" << number
           << ", Sequence=" << keysSetSequenceNumber
           << ", PublicKeySize=" << publicKey->keySize()
           << ", PrivateKeySize=" << PrivateKey::keySize();
#endif
}

const KeyNumber OwnKeysHandler::maxKeySetSequenceNumber(
    const TrustLineID trustLineID)
{
    string query = "SELECT MAX(keys_set_sequence_number) FROM " + mTableName + " WHERE trust_line_id = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::maxKeySetSequenceNumber: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        // Check if result is NULL
        if (sqlite3_column_type(stmt.get(), 0) == SQLITE_NULL) {
            throw NotFoundError("OwnKeysHandler::maxKeySetSequenceNumber: No keys found. "
                                "TrustLine=" + to_string(trustLineID) + ".");
        }

        KeyNumber maxSequence = (KeyNumber)sqlite3_column_int(stmt.get(), 0);

#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Max sequence retrieved: TrustLine=" << trustLineID
               << ", MaxSequence=" << maxSequence;
#endif
        return maxSequence;
    } else {
        throw IOError("OwnKeysHandler::maxKeySetSequenceNumber: Failed to execute SELECT. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

pair<std::unique_ptr<PrivateKey>, KeyNumber> OwnKeysHandler::nextAvailableKey(
    const TrustLineID trustLineID)
{
    string query = "SELECT private_key, number FROM " + mTableName +
                   " WHERE trust_line_id = ? AND is_valid = 1 ORDER BY number LIMIT 1;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::nextAvailableKey: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        auto privateKeyData = (byte_t*)sqlite3_column_blob(stmt.get(), 0);
        auto number = (KeyNumber)sqlite3_column_int(stmt.get(), 1);

        auto privateKey = std::make_unique<PrivateKey>(privateKeyData);

#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Next available key retrieved: TrustLine=" << trustLineID
               << ", KeyNumber=" << number;
#endif

        return make_pair(std::move(privateKey), number);
    } else if (rc == SQLITE_DONE) {
        throw NotFoundError("OwnKeysHandler::nextAvailableKey: No available keys found. "
                            "TrustLine=" + to_string(trustLineID) + ".");
    } else {
        throw IOError("OwnKeysHandler::nextAvailableKey: Failed to execute SELECT. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

void OwnKeysHandler::invalidKey(
    const TrustLineID trustLineID,
    const KeyNumber number,
    const Signature::Shared signature)
{
    if (!signature) {
        throw ValueError("OwnKeysHandler::invalidKey: Signature cannot be null.");
    }

    string query = "UPDATE " + mTableName + " SET is_valid = 0, private_key = ? WHERE trust_line_id = ? AND number = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, signature->data(),
                               (int)signature->signatureSize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::invalidKey: Failed to bind signature. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::invalidKey: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 3, number);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::invalidKey: Failed to bind number. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("OwnKeysHandler::invalidKey: Failed to execute UPDATE. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    if (sqlite3_changes(mDataBase) == 0) {
        throw ValueError("OwnKeysHandler::invalidKey: No rows affected. "
                         "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(number) + ".");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Key invalidated: TrustLine=" << trustLineID
           << ", KeyNumber=" << number
           << ", SignatureSize=" << signature->signatureSize();
#endif
}

void OwnKeysHandler::invalidateKeyByHash(
    const TrustLineID trustLineID,
    const KeyHash::Shared keyHash,
    const Signature::Shared signature)
{
    if (!keyHash) {
        throw ValueError("OwnKeysHandler::invalidateKeyByHash: Key hash cannot be null.");
    }

    if (!signature) {
        throw ValueError("OwnKeysHandler::invalidateKeyByHash: Signature cannot be null.");
    }

    string query = "UPDATE " + mTableName + " SET is_valid = 0, private_key = ? WHERE trust_line_id = ? AND hash = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, signature->data(),
                               (int)signature->signatureSize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::invalidateKeyByHash: Failed to bind signature. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::invalidateKeyByHash: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_blob(stmt.get(), 3, keyHash->data(),
                           (int)KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::invalidateKeyByHash: Failed to bind hash. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("OwnKeysHandler::invalidateKeyByHash: Failed to execute UPDATE. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    if (sqlite3_changes(mDataBase) == 0) {
        throw ValueError("OwnKeysHandler::invalidateKeyByHash: No rows affected. "
                         "TrustLine=" + to_string(trustLineID) + ".");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Key invalidated by hash: TrustLine=" << trustLineID
           << ", SignatureSize=" << signature->signatureSize();
#endif
}

const PublicKey::Shared OwnKeysHandler::getPublicKey(
    const TrustLineID trustLineID,
    const KeyNumber keyNumber)
{
    string query = "SELECT public_key FROM " + mTableName +
                   " WHERE trust_line_id = ? AND number = ? AND is_valid = 1;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::getPublicKey: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(keyNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, keyNumber);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::getPublicKey: Failed to bind number. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(keyNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        auto result = make_shared<PublicKey>((byte_t*)sqlite3_column_blob(stmt.get(), 0));

#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Public key retrieved: TrustLine=" << trustLineID
               << ", KeyNumber=" << keyNumber;
#endif
        return result;
    } else if (rc == SQLITE_DONE) {
        throw NotFoundError("OwnKeysHandler::getPublicKey: Key not found. "
                            "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(keyNumber) + ".");
    } else {
        throw IOError("OwnKeysHandler::getPublicKey: Failed to execute SELECT. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(keyNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

const PublicKey::Shared OwnKeysHandler::getPublicKeyByHash(
    const TrustLineID trustLineID,
    const KeyHash::Shared keyHash)
{
    if (!keyHash) {
        throw ValueError("OwnKeysHandler::getPublicKeyByHash: Key hash cannot be null.");
    }

    string query = "SELECT public_key FROM " + mTableName +
                   " WHERE trust_line_id = ? AND hash = ? AND is_valid = 1;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::getPublicKeyByHash: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_blob(stmt.get(), 2, keyHash->data(),
                           (int)KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::getPublicKeyByHash: Failed to bind hash. "
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
        throw NotFoundError("OwnKeysHandler::getPublicKeyByHash: Key not found. "
                            "TrustLine=" + to_string(trustLineID) + ".");
    } else {
        throw IOError("OwnKeysHandler::getPublicKeyByHash: Failed to execute SELECT. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

const KeyHash::Shared OwnKeysHandler::getPublicKeyHash(
    const TrustLineID trustLineID,
    const KeyNumber keyNumber)
{
    string query = "SELECT hash FROM " + mTableName +
                   " WHERE trust_line_id = ? AND number = ? AND is_valid = 1;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::getPublicKeyHash: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(keyNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, keyNumber);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::getPublicKeyHash: Failed to bind number. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(keyNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        auto result = make_shared<KeyHash>((byte_t*)sqlite3_column_blob(stmt.get(), 0));

#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Key hash retrieved: TrustLine=" << trustLineID
               << ", KeyNumber=" << keyNumber;
#endif
        return result;
    } else if (rc == SQLITE_DONE) {
        throw NotFoundError("OwnKeysHandler::getPublicKeyHash: Key hash not found. "
                            "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(keyNumber) + ".");
    } else {
        throw IOError("OwnKeysHandler::getPublicKeyHash: Failed to execute SELECT. "
                      "TrustLine=" + to_string(trustLineID) + ", KeyNumber=" + to_string(keyNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

const KeyNumber OwnKeysHandler::getKeyNumberByHash(
    const KeyHash::Shared keyHash)
{
    if (!keyHash) {
        throw ValueError("OwnKeysHandler::getKeyNumberByHash: Key hash cannot be null.");
    }

    string query = "SELECT number FROM " + mTableName + " WHERE hash = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, keyHash->data(),
                               (int)KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::getKeyNumberByHash: Failed to bind hash. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        KeyNumber result = (KeyNumber)sqlite3_column_int(stmt.get(), 0);

#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Key number retrieved by hash: KeyNumber=" << result;
#endif
        return result;
    } else if (rc == SQLITE_DONE) {
        throw NotFoundError("OwnKeysHandler::getKeyNumberByHash: Key number not found for given hash.");
    } else {
        throw IOError("OwnKeysHandler::getKeyNumberByHash: Failed to execute SELECT. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

KeysCount OwnKeysHandler::availableKeysCnt(
    const TrustLineID trustLineID)
{
    string query = "SELECT count(*) FROM " + mTableName +
                   " WHERE trust_line_id = ? AND is_valid = 1";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::availableKeysCnt: Failed to bind trust_line_id. "
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
        throw IOError("OwnKeysHandler::availableKeysCnt: Failed to execute count query. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

void OwnKeysHandler::removeUnusedKeys(
    const TrustLineID trustLineID)
{
    string query = "DELETE FROM " + mTableName + " WHERE trust_line_id = ? AND is_valid = 1";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::removeUnusedKeys: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("OwnKeysHandler::removeUnusedKeys: Failed to execute DELETE. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    int deletedRows = sqlite3_changes(mDataBase);

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Unused keys removed: TrustLine=" << trustLineID
           << ", DeletedCount=" << deletedRows;
#endif
}

vector<PublicKey::Shared> OwnKeysHandler::publicKeysBySetNumber(
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
        throw IOError("OwnKeysHandler::publicKeysBySetNumber: Failed to bind trust_line_id in count query. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(countStmt.get(), 2, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::publicKeysBySetNumber: Failed to bind sequence_number in count query. "
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
        throw IOError("OwnKeysHandler::publicKeysBySetNumber: Failed to bind trust_line_id in data query. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::publicKeysBySetNumber: Failed to bind sequence_number in data query. "
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

void OwnKeysHandler::deleteKeysByTrustLineID(
    const TrustLineID trustLineID)
{
    string query = "DELETE FROM " + mTableName + " WHERE trust_line_id = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::deleteKeysByTrustLineID: Failed to bind trust_line_id. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("OwnKeysHandler::deleteKeysByTrustLineID: Failed to execute DELETE. "
                      "TrustLine=" + to_string(trustLineID) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    int deletedRows = sqlite3_changes(mDataBase);

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "All keys deleted: TrustLine=" << trustLineID
           << ", DeletedCount=" << deletedRows;
#endif
}

void OwnKeysHandler::deleteKeyByHashExceptSequenceNumber(
    KeyHash::Shared keyHash,
    const KeyNumber keysSetSequenceNumber)
{
    if (!keyHash) {
        throw ValueError("OwnKeysHandler::deleteKeyByHashExceptSequenceNumber: Key hash cannot be null.");
    }

    string query = "DELETE FROM " + mTableName +
                   " WHERE hash = ? AND keys_set_sequence_number != ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int rc = sqlite3_bind_blob(stmt.get(), 1, keyHash->data(),
                               (int)KeyHash::kBytesSize, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::deleteKeyByHashExceptSequenceNumber: Failed to bind hash. "
                      "Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::deleteKeyByHashExceptSequenceNumber: Failed to bind sequence_number. "
                      "Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("OwnKeysHandler::deleteKeyByHashExceptSequenceNumber: Failed to execute DELETE. "
                      "Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    int deletedRows = sqlite3_changes(mDataBase);

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Keys deleted by hash: PreservedSequence=" << keysSetSequenceNumber
           << ", DeletedCount=" << deletedRows;
#endif
}

vector<KeyHash::Shared> OwnKeysHandler::publicKeyHashesLessThanSetNumber(
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
        throw IOError("OwnKeysHandler::publicKeyHashesLessThanSetNumber: Failed to bind trust_line_id in count query. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(countStmt.get(), 2, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::publicKeyHashesLessThanSetNumber: Failed to bind sequence_number in count query. "
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
        throw IOError("OwnKeysHandler::publicKeyHashesLessThanSetNumber: Failed to bind trust_line_id in data query. "
                      "TrustLine=" + to_string(trustLineID) + ", Sequence=" + to_string(keysSetSequenceNumber) +
                      ". SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    rc = sqlite3_bind_int(stmt.get(), 2, keysSetSequenceNumber);
    if (rc != SQLITE_OK) {
        throw IOError("OwnKeysHandler::publicKeyHashesLessThanSetNumber: Failed to bind sequence_number in data query. "
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

LoggerStream OwnKeysHandler::info() const
{
    return mLog.info(logHeader());
}

LoggerStream OwnKeysHandler::warning() const
{
    return mLog.warning(logHeader());
}

const string OwnKeysHandler::logHeader() const
{
    stringstream s;
    s << "[OwnKeysHandler: (" << mTableName << ")]";
    return s.str();
}
