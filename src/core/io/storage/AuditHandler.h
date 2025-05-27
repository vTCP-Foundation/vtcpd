#ifndef VTCPD_AUDITHANDLER_H
#define VTCPD_AUDITHANDLER_H

#include "../../logger/Logger.h"
#include "../../common/Types.h"
#include "../../common/multiprecision/MultiprecisionUtils.h"
#include "../../common/exceptions/IOError.h"
#include "../../common/exceptions/NotFoundError.h"
#include "../../common/exceptions/ValueError.h"
#include "../../crypto/lamportscheme.h"
#include "record/audit/AuditRecord.h"

#include <sqlite3.h>

using namespace crypto;

class AuditHandler
{

public:
    AuditHandler(
        sqlite3 *dbConnection,
        const string &tableName,
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
        const TrustLineBalance &balance);

    void saveOwnAuditPart(
        AuditNumber number,
        TrustLineID trustLineID,
        lamport::KeyHash::Shared ownKeyHash,
        lamport::Signature::Shared ownSignature,
        lamport::KeyHash::Shared ownKeysSetHash,
        lamport::KeyHash::Shared contractorKeysSetHash,
        const TrustLineAmount &incomingAmount,
        const TrustLineAmount &outgoingAmount,
        const TrustLineBalance &balance);

    void saveContractorAuditPart(
        AuditNumber number,
        TrustLineID trustLineID,
        lamport::KeyHash::Shared contractorKeyHash,
        lamport::Signature::Shared contractorSignature);

    const AuditRecord::Shared getActualAudit(
        TrustLineID trustLineID);

    const AuditRecord::Shared getActualAuditFull(
        TrustLineID trustLineID);

    const AuditNumber getActualAuditNumber(
        TrustLineID trustLineID);

    void deleteRecords(
        TrustLineID trustLineID);

    void deleteAuditByNumber(
        TrustLineID trustLineID,
        AuditNumber auditNumber);

    vector<AuditRecord::Shared> auditsLessEqualThanAuditNumber(
        TrustLineID trustLineID,
        AuditNumber auditNumber);

    bool isContainsKeyHash(
        lamport::KeyHash::Shared keyHash) const;

private:
    LoggerStream info() const;

    LoggerStream warning() const;

    const string logHeader() const;

private:
    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger &mLog;
};


#endif //VTCPD_AUDITHANDLER_H
