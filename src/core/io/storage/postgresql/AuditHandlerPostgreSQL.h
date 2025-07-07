#ifndef VTCPD_AUDITHANDLERPOSTGRESQL_H
#define VTCPD_AUDITHANDLERPOSTGRESQL_H

#include "../interfaces/AuditHandler.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../crypto/lamportkeys.h"
#include "../../../common/memory/MemoryUtils.h"
#include <libpq-fe.h>
#include <memory>

using namespace crypto::lamport;

class AuditHandlerPostgreSQL : public AuditHandler
{
public:
    AuditHandlerPostgreSQL(
        PGconn *dbConnection,
        const std::string &tableName,
        Logger &logger);

    void saveFullAudit(
        AuditNumber number,
        TrustLineID trustLineID,
        lamport::KeyHash::Shared ownKeyHash,
        lamport::Signature::Shared ownSignature,
        lamport::KeyHash::Shared contractorKeyHash,
        lamport::Signature::Shared contractorSignature,
        lamport::KeyHash::Shared ownKeysSetHash,
        lamport::KeyHash::Shared contractorKeysSetHash,
        const TrustLineAmount &incomingAmount,
        const TrustLineAmount &outgoingAmount,
        const TrustLineBalance &balance) override;

    void saveOwnAuditPart(
        AuditNumber number,
        TrustLineID trustLineID,
        lamport::KeyHash::Shared ownKeyHash,
        lamport::Signature::Shared ownSignature,
        lamport::KeyHash::Shared ownKeysSetHash,
        lamport::KeyHash::Shared contractorKeysSetHash,
        const TrustLineAmount &incomingAmount,
        const TrustLineAmount &outgoingAmount,
        const TrustLineBalance &balance) override;

    void saveContractorAuditPart(
        AuditNumber number,
        TrustLineID trustLineID,
        lamport::KeyHash::Shared contractorKeyHash,
        lamport::Signature::Shared contractorSignature) override;

    const AuditRecord::Shared getActualAudit(
        TrustLineID trustLineID) override;

    const AuditRecord::Shared getActualAuditFull(
        TrustLineID trustLineID) override;

    const AuditNumber getActualAuditNumber(
        TrustLineID trustLineID) override;

    void deleteRecords(
        TrustLineID trustLineID) override;

    void deleteAuditByNumber(
        TrustLineID trustLineID,
        AuditNumber auditNumber) override;

    std::vector<AuditRecord::Shared> auditsLessEqualThanAuditNumber(
        TrustLineID trustLineID,
        AuditNumber auditNumber) override;

    bool isContainsKeyHash(
        lamport::KeyHash::Shared keyHash) const override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const std::string logHeader() const;

    PGconn *mDataBase = nullptr;
    std::string mTableName;
    Logger &mLog;
};

#endif // VTCPD_AUDITHANDLERPOSTGRESQL_H 