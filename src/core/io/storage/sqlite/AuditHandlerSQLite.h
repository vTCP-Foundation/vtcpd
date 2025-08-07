#ifndef VTCPD_AUDITHANDLERSQLITE_H
#define VTCPD_AUDITHANDLERSQLITE_H

#include "../../../logger/Logger.h"
#include "../interfaces/AuditHandler.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../crypto/sphincskeys.h"
#include "../../../common/memory/MemoryUtils.h"
#include "SQLiteStatementRAII.h"
#include <sqlite3.h>
#include <memory>

using namespace crypto::sphincs;

class AuditHandlerSQLite : public AuditHandler
{
public:
    AuditHandlerSQLite(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger &logger);

    void saveFullAudit(
        AuditNumber number,
        TrustLineID trustLineID,
        sphincs::KeyHash::Shared ownKeyHash,
        sphincs::Signature::Shared ownSignature,
        sphincs::KeyHash::Shared contractorKeyHash,
        sphincs::Signature::Shared contractorSignature,
        sphincs::KeyHash::Shared ownKeysSetHash,
        sphincs::KeyHash::Shared contractorKeysSetHash,
        const TrustLineAmount &incomingAmount,
        const TrustLineAmount &outgoingAmount,
        const TrustLineBalance &balance) override;

    void saveOwnAuditPart(
        AuditNumber number,
        TrustLineID trustLineID,
        sphincs::KeyHash::Shared ownKeyHash,
        sphincs::Signature::Shared ownSignature,
        sphincs::KeyHash::Shared ownKeysSetHash,
        sphincs::KeyHash::Shared contractorKeysSetHash,
        const TrustLineAmount &incomingAmount,
        const TrustLineAmount &outgoingAmount,
        const TrustLineBalance &balance) override;

    void saveContractorAuditPart(
        AuditNumber number,
        TrustLineID trustLineID,
        sphincs::KeyHash::Shared contractorKeyHash,
        sphincs::Signature::Shared contractorSignature) override;

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

    vector<AuditRecord::Shared> auditsLessEqualThanAuditNumber(
        TrustLineID trustLineID,
        AuditNumber auditNumber) override;

    bool isContainsKeyHash(
        sphincs::KeyHash::Shared keyHash) const override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const string logHeader() const;

    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger &mLog;
};

#endif //VTCPD_AUDITHANDLERSQLITE_H
