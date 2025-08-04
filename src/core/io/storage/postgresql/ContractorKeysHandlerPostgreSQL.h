#ifndef VTCPD_CONTRACTORKEYSHANDLERPOSTGRESQL_H
#define VTCPD_CONTRACTORKEYSHANDLERPOSTGRESQL_H

#include "../interfaces/ContractorKeysHandler.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../crypto/lamportkeys.h"
#include "../../../crypto/lamportscheme.h"
#include "../../../common/memory/MemoryUtils.h"
#include <libpq-fe.h>
#include <memory>
#include <vector>

using namespace crypto::lamport;

class ContractorKeysHandlerPostgreSQL : public ContractorKeysHandler
{
public:
    ContractorKeysHandlerPostgreSQL(
        PGconn *dbConnection,
        const std::string &tableName,
        Logger &logger);

    void saveKey(
        const TrustLineID trustLineID,
        const KeyNumber keysSetSequenceNumber,
        const PublicKey::Shared publicKey,
        const KeyNumber number) override;

    const KeyNumber maxKeySetSequenceNumber(
        const TrustLineID trustLineID) override;

    void invalidKey(
        const TrustLineID trustLineID,
        const KeyNumber number) override;

    void invalidateKeyByHash(
        const TrustLineID trustLineID,
        const KeyHash::Shared keyHash) override;

    PublicKey::Shared keyByNumber(
        const TrustLineID trustLineID,
        const KeyNumber keyNumber) override;

    PublicKey::Shared keyByHash(
        const TrustLineID trustLineID,
        const KeyHash::Shared keyHash) override;

    const KeyHash::Shared keyHashByNumber(
        const TrustLineID trustLineID,
        const KeyNumber keyNumber) override;

    KeysCount availableKeysCnt(
        const TrustLineID trustLineID) override;

    KeysCount sequenceKeysCnt(
        const TrustLineID trustLineID,
        KeyNumber keysSetSequenceNumber) override;

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

#endif // VTCPD_CONTRACTORKEYSHANDLERPOSTGRESQL_H 