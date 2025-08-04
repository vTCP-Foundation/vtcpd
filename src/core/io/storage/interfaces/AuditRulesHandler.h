#ifndef VTCPD_INTERFACES_AUDITRULESHANDLER_H
#define VTCPD_INTERFACES_AUDITRULESHANDLER_H

#include "../../../logger/Logger.h"
#include "../../../common/Types.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../trust_lines/audit_rules/BaseAuditRule.h"

#include <memory>

using namespace std;

class AuditRulesHandler
{
public:
    virtual ~AuditRulesHandler() = default;

    virtual void saveRule(
        TrustLineID trustLineID,
        BaseAuditRule::AuditRuleType auditRuleType) = 0;

    virtual const BaseAuditRule::AuditRuleType getRule(
        TrustLineID trustLineID) = 0;

    virtual void removeAuditRules(
        TrustLineID trustLineID) = 0;
};

#endif //VTCPD_INTERFACES_AUDITRULESHANDLER_H 