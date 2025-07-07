#include "AuditRulesHandlerPostgreSQL.h"
#include <sstream>

using namespace std;

namespace {
inline void checkCmd(PGconn *db, PGresult *res, const string &prefix) {
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        string err = PQerrorMessage(db);
        PQclear(res);
        throw IOError(prefix + ": " + err);
    }
}
inline void checkTuples(PGconn *db, PGresult *res, const string &prefix) {
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        string err = PQerrorMessage(db);
        PQclear(res);
        throw IOError(prefix + ": " + err);
    }
}
}

AuditRulesHandlerPostgreSQL::AuditRulesHandlerPostgreSQL(
    PGconn *dbConnection,
    const string &tableName,
    Logger &logger) :
    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    if (mDataBase == nullptr) {
        throw IOError("AuditRulesHandlerPostgreSQL: Database connection is null");
    }

    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   "(trust_line_id INTEGER NOT NULL, "
                   "rule_id INTEGER NOT NULL, "
                   "parameters BYTEA, "
                   "FOREIGN KEY(trust_line_id) REFERENCES trust_lines(id) ON DELETE CASCADE ON UPDATE CASCADE);";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase, res, "AuditRulesHandlerPostgreSQL::creating table");
    PQclear(res);

    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName + "_trust_line_id_idx on " + mTableName + "(trust_line_id);";
    res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase, res, "AuditRulesHandlerPostgreSQL::creating index");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "AuditRulesHandler initialized: table=" << mTableName;
#endif
}

void AuditRulesHandlerPostgreSQL::saveRule(
    TrustLineID trustLineID,
    BaseAuditRule::AuditRuleType auditRuleType)
{
    const string query = "INSERT INTO " + mTableName + "(trust_line_id, rule_id) VALUES ($1, $2);";
    const char *params[2];
    int lengths[2] = {0,0};
    int formats[2] = {0,0};
    string tlIdStr = to_string(trustLineID);
    string ruleStr = to_string(static_cast<int>(auditRuleType));
    params[0] = tlIdStr.c_str();
    params[1] = ruleStr.c_str();

    PGresult *res = PQexecParams(mDataBase, query.c_str(), 2, nullptr, params, lengths, formats, 0);
    checkCmd(mDataBase, res, "AuditRulesHandlerPostgreSQL::saveRule");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "prepare inserting is completed successfully";
#endif
}

const BaseAuditRule::AuditRuleType AuditRulesHandlerPostgreSQL::getRule(
    TrustLineID trustLineID)
{
    const string query = "SELECT rule_id FROM " + mTableName + " WHERE trust_line_id = $1;";
    const char *params[1];
    int lengths[1] = {0};
    int formats[1] = {0};
    string tlIdStr = to_string(trustLineID);
    params[0] = tlIdStr.c_str();

    PGresult *res = PQexecParams(mDataBase, query.c_str(), 1, nullptr, params, lengths, formats, 0);
    checkTuples(mDataBase, res, "AuditRulesHandlerPostgreSQL::getRule");

    if (PQntuples(res) == 0) {
        PQclear(res);
        throw NotFoundError("No records with requested trust line id");
    }
    int ruleId = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return static_cast<BaseAuditRule::AuditRuleType>(ruleId);
}

void AuditRulesHandlerPostgreSQL::removeAuditRules(
    TrustLineID trustLineID)
{
    const string query = "DELETE FROM " + mTableName + " WHERE trust_line_id = $1;";
    const char *params[1];
    int lengths[1] = {0};
    int formats[1] = {0};
    string tlIdStr = to_string(trustLineID);
    params[0] = tlIdStr.c_str();

    PGresult *res = PQexecParams(mDataBase, query.c_str(), 1, nullptr, params, lengths, formats, 0);
    checkCmd(mDataBase, res, "AuditRulesHandlerPostgreSQL::removeAuditRules");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "deleting is completed successfully";
#endif
}

LoggerStream AuditRulesHandlerPostgreSQL::info() const
{
    return mLog.info(logHeader());
}

LoggerStream AuditRulesHandlerPostgreSQL::warning() const
{
    return mLog.warning(logHeader());
}

const string AuditRulesHandlerPostgreSQL::logHeader() const
{
    stringstream s;
    s << "[AuditRulesHandlerPostgreSQL]";
    return s.str();
} 