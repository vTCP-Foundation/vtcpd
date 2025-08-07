#ifndef VTCPD_CONTRACTORKEYSHANDLERSQLITE_H
#define VTCPD_CONTRACTORKEYSHANDLERSQLITE_H

#include "../../../logger/Logger.h"
#include "../interfaces/ContractorKeysHandler.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../crypto/sphincskeys.h"
#include "../../../crypto/sphincsscheme.h"
#include "../../../common/memory/MemoryUtils.h"
#include "SQLiteStatementRAII.h"
#include <sqlite3.h>
#include <memory>

using namespace crypto::sphincs;

class ContractorKeysHandlerSQLite : public ContractorKeysHandler
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
     * Also creates necessary indexes on hash and trust_line_id columns.
     * @param dbConnection SQLite database connection (must not be null)
     * @param tableName Name of the table to create/use (must not be empty)
     * @param logger Logger instance for debugging and error reporting
     * @throws ValueError if dbConnection is null or tableName is empty
     * @throws IOError if database operations fail
     */
    ContractorKeysHandlerSQLite(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger &logger);

    /**
     * Saves a public key to the database with associated metadata.
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
     * @return Maximum sequence number
     * @throws NotFoundError if no keys found for the trust line
     */
    const KeyNumber maxKeySetSequenceNumber(
        const TrustLineID trustLineID);

    /**
     * Marks a key as invalid by trust line ID and key number.
     * @param number Key number to invalidate
     * @throws ValueError if no data was changed
     */
    void invalidKey(
        const TrustLineID trustLineID,
        const KeyNumber number) override;

    /**
     * Marks a key as invalid by hash.
     * @param keyHash Hash of the key to invalidate (must not be null)
     * @throws ValueError if keyHash is null or no data was changed
     */
    void invalidateKeyByHash(
        const TrustLineID trustLineID,
        const KeyHash::Shared keyHash) override;

    /**
     * Retrieves a public key by trust line ID and key number.
     * @param keyNumber Key number
     * @return Shared pointer to the public key
     * @throws NotFoundError if key not found
     */
    PublicKey::Shared keyByNumber(
        const TrustLineID trustLineID,
        const KeyNumber keyNumber) override;

    /**
     * Retrieves a public key by trust line ID and key hash.
     * @param keyHash Hash of the key to retrieve (must not be null)
     * @return Shared pointer to the public key
     * @throws ValueError if keyHash is null
     * @throws NotFoundError if key not found
     */
    PublicKey::Shared keyByHash(
        const TrustLineID trustLineID,
        const KeyHash::Shared keyHash) override;

    /**
     * Retrieves a key hash by trust line ID and key number.
     * @return Shared pointer to the key hash
     * @throws NotFoundError if key hash not found
     */
    const KeyHash::Shared keyHashByNumber(
        const TrustLineID trustLineID,
        const KeyNumber keyNumber) override;

    /**
     * Counts available (valid) keys for a trust line.
     * @return Number of available keys
     */
    KeysCount availableKeysCnt(
        const TrustLineID trustLineID) override;

    /**
     * Counts keys for a specific sequence number.
     * @param keysSetSequenceNumber Sequence number
     * @return Number of keys in the sequence
     */
    KeysCount sequenceKeysCnt(
        const TrustLineID trustLineID,
        KeyNumber keysSetSequenceNumber) override;

    /**
     * Removes all unused keys for a trust line.
     */
    void removeUnusedKeys(
        const TrustLineID trustLineID) override;

    /**
     * Retrieves all public keys for a specific sequence number.
     * @param keysSetSequenceNumber Sequence number
     * @return Vector of public keys ordered by key number
     */
    vector<PublicKey::Shared> publicKeysBySetNumber(
        const TrustLineID trustLineID,
        const KeyNumber keysSetSequenceNumber) const override;

    /**
     * Deletes all keys associated with a trust line.
     */
    void deleteKeysByTrustLineID(
        const TrustLineID trustLineID) override;

    /**
     * Deletes keys by hash except those in a specific sequence.
     * @param keyHash Hash of keys to delete (must not be null)
     * @param keysSetSequenceNumber Sequence number to preserve
     */
    void deleteKeyByHashExceptSequenceNumber(
        KeyHash::Shared keyHash,
        const KeyNumber keysSetSequenceNumber) override;

    /**
     * Retrieves key hashes for sequences less than the specified number.
     * @param keysSetSequenceNumber Upper bound sequence number (exclusive)
     * @return Vector of key hashes
     */
    vector<KeyHash::Shared> publicKeyHashesLessThanSetNumber(
        const TrustLineID trustLineID,
        const KeyNumber keysSetSequenceNumber) const override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const string logHeader() const;

    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger &mLog;
};

#endif //VTCPD_CONTRACTORKEYSHANDLERSQLITE_H
