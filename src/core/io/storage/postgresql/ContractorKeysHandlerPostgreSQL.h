#ifndef VTCPD_CONTRACTORKEYSHANDLERPOSTGRESQL_H
#define VTCPD_CONTRACTORKEYSHANDLERPOSTGRESQL_H

#include "../interfaces/ContractorKeysHandler.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../crypto/sphincskeys.h"
#include "../../../crypto/sphincsscheme.h"
#include "../../../common/memory/MemoryUtils.h"
#include <libpq-fe.h>
#include <memory>
#include <vector>

using namespace crypto::sphincs;

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
        const PublicKey::Shared publicKey) override;

    const KeyNumber maxKeySetSequenceNumber(
        const TrustLineID trustLineID) override;

    void invalidateKey(
        const TrustLineID trustLineID) override;

    void invalidateKeyByHash(
        const TrustLineID trustLineID,
        const KeyHash::Shared keyHash) override;

    PublicKey::Shared getPublicKey(
        const TrustLineID trustLineID) override;

    PublicKey::Shared keyByHash(
        const TrustLineID trustLineID,
        const KeyHash::Shared keyHash) override;

    const KeyHash::Shared getPublicKeyHash(
        const TrustLineID trustLineID) override;

    bool hasKey(
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