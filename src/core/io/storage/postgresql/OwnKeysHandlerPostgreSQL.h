#ifndef VTCPD_OWNKEYSHANDLERPOSTGRESQL_H
#define VTCPD_OWNKEYSHANDLERPOSTGRESQL_H

#include "../interfaces/OwnKeysHandler.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../crypto/sphincskeys.h"
#include "../../../crypto/sphincsscheme.h"
#include "../../../common/memory/MemoryUtils.h"

#include <libpq-fe.h>
#include <string>
#include <vector>
#include <map>

using namespace crypto::sphincs;

class OwnKeysHandlerPostgreSQL : public OwnKeysHandler
{
public:
    OwnKeysHandlerPostgreSQL(
        PGconn *dbConnection,
        const std::string &tableName,
        Logger &logger);

    void saveKey(
        const TrustLineID trustLineID,
        const KeyNumber keysSetSequenceNumber,
        const PublicKey::Shared publicKey,
        const PrivateKey *privateKey,
        const KeyNumber number) override;

    const KeyNumber maxKeySetSequenceNumber(
        const TrustLineID trustLineID) override;

    std::pair<std::unique_ptr<PrivateKey>, KeyNumber> nextAvailableKey(
        const TrustLineID trustLineID) override;

    void invalidKey(
        const TrustLineID trustLineID,
        const KeyNumber number,
        const Signature::Shared signature) override;

    void invalidateKeyByHash(
        const TrustLineID trustLineID,
        const KeyHash::Shared keyHash,
        const Signature::Shared signature) override;

    const PublicKey::Shared getPublicKey(
        const TrustLineID trustLineID,
        const KeyNumber keyNumber) override;

    const PublicKey::Shared getPublicKeyByHash(
        const TrustLineID trustLineID,
        const KeyHash::Shared keyHash) override;

    const KeyHash::Shared getPublicKeyHash(
        const TrustLineID trustLineID,
        const KeyNumber keyNumber) override;

    const KeyNumber getKeyNumberByHash(
        const KeyHash::Shared keyHash) override;

    KeysCount availableKeysCnt(
        const TrustLineID trustLineID) override;

    void removeUnusedKeys(
        const TrustLineID trustLineID) override;

    std::vector<PublicKey::Shared> publicKeysBySetNumber(
        const TrustLineID trustLineID,
        const KeyNumber keysSetSequenceNumber) const override;

    void deleteKeysByTrustLineID(
        const TrustLineID trustLineID) override;

    void deleteKeyByHashExceptSequenceNumber(
        KeyHash::Shared keyHash,
        const KeyNumber keysSetSequenceNumber) override;

    std::vector<KeyHash::Shared> publicKeyHashesLessThanSetNumber(
        const TrustLineID trustLineID,
        const KeyNumber keysSetSequenceNumber) const override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const std::string logHeader() const;

    PGconn *mDataBase = nullptr;
    std::string mTableName;
    Logger &mLog;
};

#endif // VTCPD_OWNKEYSHANDLERPOSTGRESQL_H 