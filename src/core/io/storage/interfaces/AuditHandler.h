#ifndef VTCPD_INTERFACES_AUDITHANDLER_H
#define VTCPD_INTERFACES_AUDITHANDLER_H

#include "../../../logger/Logger.h"
#include "../../../common/Types.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../crypto/sphincsscheme.h"
#include "../record/audit/AuditRecord.h"

#include <vector>
#include <memory>

using namespace std;
using namespace crypto;

class AuditHandler
{
public:
    virtual ~AuditHandler() = default;

    virtual void saveFullAudit(
        AuditNumber number,
        TrustLineID trustLineID,
        sphincs::Signature::Shared ownSignature,
        sphincs::Signature::Shared contractorSignature,
        const TrustLineAmount &incomingAmount,
        const TrustLineAmount &outgoingAmount,
        const TrustLineBalance &balance) = 0;

    virtual void saveOwnAuditPart(
        AuditNumber number,
        TrustLineID trustLineID,
        sphincs::Signature::Shared ownSignature,
        const TrustLineAmount &incomingAmount,
        const TrustLineAmount &outgoingAmount,
        const TrustLineBalance &balance) = 0;

    virtual void saveContractorAuditPart(
        AuditNumber number,
        TrustLineID trustLineID,
        sphincs::Signature::Shared contractorSignature) = 0;

    virtual const AuditRecord::Shared getActualAudit(
        TrustLineID trustLineID) = 0;

    virtual const AuditRecord::Shared getActualAuditFull(
        TrustLineID trustLineID) = 0;

    virtual const AuditNumber getActualAuditNumber(
        TrustLineID trustLineID) = 0;

    virtual void deleteRecords(
        TrustLineID trustLineID) = 0;

    virtual void deleteAuditByNumber(
        TrustLineID trustLineID,
        AuditNumber auditNumber) = 0;

    virtual vector<AuditRecord::Shared> auditsLessEqualThanAuditNumber(
        TrustLineID trustLineID,
        AuditNumber auditNumber) = 0;

};

#endif //VTCPD_INTERFACES_AUDITHANDLER_H 