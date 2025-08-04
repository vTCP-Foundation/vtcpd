#ifndef VTCPD_AUDITRULESHANDLERPOSTGRESQL_H
#define VTCPD_AUDITRULESHANDLERPOSTGRESQL_H

#include "../interfaces/AuditRulesHandler.h"
#include "../../../logger/Logger.h"
#include "../../../common/Types.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../trust_lines/audit_rules/BaseAuditRule.h"
#include <libpq-fe.h>

class AuditRulesHandlerPostgreSQL : public AuditRulesHandler
{
public:
    AuditRulesHandlerPostgreSQL(
        PGconn *dbConnection,
        const std::string &tableName,
        Logger &logger);

    void saveRule(
        TrustLineID trustLineID,
        BaseAuditRule::AuditRuleType auditRuleType) override;

    const BaseAuditRule::AuditRuleType getRule(
        TrustLineID trustLineID) override;

    void removeAuditRules(
        TrustLineID trustLineID) override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const std::string logHeader() const;

    PGconn *mDataBase = nullptr;
    std::string mTableName;
    Logger &mLog;
};

#endif // VTCPD_AUDITRULESHANDLERPOSTGRESQL_H 