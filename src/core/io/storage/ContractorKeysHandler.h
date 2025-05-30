#ifndef VTCPD_CONTRACTORKEYSHANDLER_H
#define VTCPD_CONTRACTORKEYSHANDLER_H

#include "../../logger/Logger.h"
#include "../../common/memory/MemoryUtils.h"
#include "../../common/exceptions/IOError.h"
#include "../../common/exceptions/NotFoundError.h"
#include "../../common/exceptions/ValueError.h"
#include "../../crypto/lamportkeys.h"
#include "SQLiteStatementRAII.h"

#include <sqlite3.h>

using namespace crypto::lamport;


class ContractorKeysHandler
{

public:
    /**
     * Constructs a ContractorKeysHandler for managing contractor keys in SQLite database.
     *
     * Creates the contractor keys table if it doesn't exist with schema:
     * - hash: BLOB PRIMARY KEY (hash of the public key)
     * - trust_line_id: INTEGER NOT NULL (foreign key to trust_lines table)
     * - keys_set_sequence_number: INTEGER NOT NULL
     * - public_key: BLOB NOT NULL (the actual public key data)
     * - number: INTEGER NOT NULL (key number within the set)
     * - is_valid: INTEGER NOT NULL DEFAULT 1 (validity flag)
     *
     * Also creates necessary indexes on hash and trust_line_id columns.
     *
     * @param dbConnection SQLite database connection (must not be null)
     * @param tableName Name of the table to create/use (must not be empty)
     * @param logger Logger instance for debugging and error reporting
     *
     * @throws ValueError if dbConnection is null or tableName is empty
     * @throws IOError if database operations fail
     */
    ContractorKeysHandler(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger &logger);

    /**
     * Saves a public key to the database with associated metadata.
     *
     * @param trustLineID Trust line identifier
     * @param keysSetSequenceNumber Sequence number of the key set
     * @param publicKey Public key to save (must not be null)
     * @param number Key number within the set
     * @throws ValueError if publicKey is null
     * @throws IOError if database operation fails
     */
    void saveKey(
        const TrustLineID trustLineID,
        const KeyNumber keysSetSequenceNumber,
        const PublicKey::Shared publicKey,
        const KeyNumber number);

    /**
     * Retrieves the maximum key set sequence number for a trust line.
     *
     * @param trustLineID Trust line identifier
     * @return Maximum sequence number
     * @throws NotFoundError if no keys found for the trust line
     * @throws IOError if database operation fails
     */
    const KeyNumber maxKeySetSequenceNumber(
        const TrustLineID trustLineID);

    /**
     * Marks a key as invalid by trust line ID and key number.
     *
     * @param trustLineID Trust line identifier
     * @param number Key number to invalidate
     * @throws ValueError if no data was changed
     * @throws IOError if database operation fails
     */
    void invalidKey(
        const TrustLineID trustLineID,
        const KeyNumber number);

    /**
     * Marks a key as invalid by trust line ID and key hash.
     *
     * @param trustLineID Trust line identifier
     * @param keyHash Hash of the key to invalidate (must not be null)
     * @throws ValueError if keyHash is null or no data was changed
     * @throws IOError if database operation fails
     */
    void invalidateKeyByHash(
        const TrustLineID trustLineID,
        const KeyHash::Shared keyHash);

    /**
     * Retrieves a public key by trust line ID and key number.
     *
     * @param trustLineID Trust line identifier
     * @param number Key number
     * @return Shared pointer to the public key
     * @throws NotFoundError if key not found
     * @throws IOError if database operation fails
     */
    PublicKey::Shared keyByNumber(
        const TrustLineID trustLineID,
        const KeyNumber number);

    /**
     * Retrieves a public key by trust line ID and key hash.
     *
     * @param trustLineID Trust line identifier
     * @param keyHash Hash of the key to retrieve (must not be null)
     * @return Shared pointer to the public key
     * @throws ValueError if keyHash is null
     * @throws NotFoundError if key not found
     * @throws IOError if database operation fails
     */
    PublicKey::Shared keyByHash(
        const TrustLineID trustLineID,
        const KeyHash::Shared keyHash);

    /**
     * Retrieves a key hash by trust line ID and key number.
     *
     * @param trustLineID Trust line identifier
     * @param number Key number
     * @return Shared pointer to the key hash
     * @throws NotFoundError if key not found
     * @throws IOError if database operation fails
     */
    const KeyHash::Shared keyHashByNumber(
        const TrustLineID trustLineID,
        const KeyNumber number);

    /**
     * Counts available (valid) keys for a trust line.
     *
     * @param trustLineID Trust line identifier
     * @return Number of available keys
     * @throws IOError if database operation fails
     */
    KeysCount availableKeysCnt(
        const TrustLineID trustLineID);

    /**
     * Counts keys in a specific sequence for a trust line.
     *
     * @param trustLineID Trust line identifier
     * @param keysSetSequenceNumber Sequence number to count
     * @return Number of keys in the sequence
     * @throws IOError if database operation fails
     */
    KeysCount sequenceKeysCnt(
        const TrustLineID trustLineID,
        KeyNumber keysSetSequenceNumber);

    /**
     * Removes all unused keys for a trust line.
     *
     * @param trustLineID Trust line identifier
     * @throws IOError if database operation fails
     */
    void removeUnusedKeys(
        const TrustLineID trustLineID);

    /**
     * Retrieves all public keys for a specific sequence number.
     *
     * @param trustLineID Trust line identifier
     * @param keysSetSequenceNumber Sequence number
     * @return Vector of public keys ordered by key number
     * @throws IOError if database operation fails
     */
    vector<PublicKey::Shared> publicKeysBySetNumber(
        const TrustLineID trustLineID,
        const KeyNumber keysSetSequenceNumber) const;

    /**
     * Deletes all keys associated with a trust line.
     *
     * @param trustLineID Trust line identifier
     * @throws IOError if database operation fails
     */
    void deleteKeysByTrustLineID(
        const TrustLineID trustLineID);

    /**
     * Deletes keys by hash except those in a specific sequence.
     *
     * @param keyHash Hash of keys to delete (must not be null)
     * @param keysSetSequenceNumber Sequence number to preserve
     * @throws ValueError if keyHash is null
     * @throws IOError if database operation fails
     */
    void deleteKeyByHashExceptSequenceNumber(
        KeyHash::Shared keyHash,
        const KeyNumber keysSetSequenceNumber);

    /**
     * Retrieves key hashes for sequences less than the specified number.
     *
     * @param trustLineID Trust line identifier
     * @param keysSetSequenceNumber Upper bound sequence number (exclusive)
     * @return Vector of key hashes
     * @throws IOError if database operation fails
     */
    vector<KeyHash::Shared> publicKeyHashesLessThanSetNumber(
        const TrustLineID trustLineID,
        const KeyNumber keysSetSequenceNumber) const;

private:
    LoggerStream info() const;

    LoggerStream warning() const;

    const string logHeader() const;

private:
    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger &mLog;
};


#endif //VTCPD_CONTRACTORKEYSHANDLER_H
