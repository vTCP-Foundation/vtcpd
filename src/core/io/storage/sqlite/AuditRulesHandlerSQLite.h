#ifndef VTCPD_AUDITRULESHANDLERSQLITE_H
#define VTCPD_AUDITRULESHANDLERSQLITE_H

#include "../../../logger/Logger.h"
#include "../interfaces/AuditRulesHandler.h"
#include "../../../common/Types.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../trust_lines/audit_rules/BaseAuditRule.h"
#include <sqlite3.h>

class AuditRulesHandlerSQLite : public AuditRulesHandler
{
public:
    AuditRulesHandlerSQLite(
        sqlite3 *dbConnection,
        const string &tableName,
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
    const string logHeader() const;
    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger &mLog;
};

#endif //VTCPD_AUDITRULESHANDLERSQLITE_H
