#ifndef VTCPD_BASEAUDITRULE_H
#define VTCPD_BASEAUDITRULE_H

#include "../../common/Types.h"
#include "../TrustLine.h"
#include "../../io/storage/interfaces/IOTransaction.h"

class BaseAuditRule
{

public:
    typedef shared_ptr<BaseAuditRule> Shared;

    enum AuditRuleType
    {
        AuditRuleCountPaymentsType = 0,
        AuditRuleTimeType = 1,
        AuditRuleTrustLineAmountBoundaryType = 2,
    };

    typedef uint16_t SerializedAuditRuleType;

    BaseAuditRule(
        AuditRuleType auditRuleType);

public:
    const AuditRuleType auditRuleType() const;

    virtual bool check(
        TrustLine::Shared trustLine,
        IOTransaction::Shared ioTransaction=nullptr) = 0;

    virtual void reset() = 0;

private:
    AuditRuleType mType;
};


#endif //VTCPD_BASEAUDITRULE_H
