#include "AuditRulesHandlerSQLite.h"

AuditRulesHandlerSQLite::AuditRulesHandlerSQLite(
    sqlite3 *dbConnection,
    const string &tableName,
    Logger &logger) :

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   "(trust_line_id INTEGER NOT NULL, "
                   "rule_id INTEGER NOT NULL, "
                   "parameters BLOB, "
                   "FOREIGN KEY(trust_line_id) REFERENCES trust_lines(id) ON DELETE CASCADE ON UPDATE CASCADE);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("AuditRulesHandlerSQLite::creating table: "
                      "Run query; sqlite error: " + to_string(rc));
    }

    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName
            + "_trust_line_id_idx on " + mTableName + "(trust_line_id);";
    SQLiteStatementRAII stmtUnique(mDataBase, query.c_str());
    rc = sqlite3_step(stmtUnique.get());
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("AuditRulesHandlerSQLite::creating index for TrustLineID: "
                      "Run query; sqlite error: " + to_string(rc));
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "AuditRulesHandler initialized: table=" << mTableName;
#endif
}

void AuditRulesHandlerSQLite::saveRule(
    TrustLineID trustLineID,
    BaseAuditRule::AuditRuleType auditRuleType)
{
    string query = "INSERT INTO " + mTableName +
                   "(trust_line_id, rule_id) VALUES (?, ?);";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("AuditRulesHandlerSQLite::saveRule: "
                      "Bad binding of TrustLineID; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 2, auditRuleType);
    if (rc != SQLITE_OK) {
        throw IOError("AuditRulesHandlerSQLite::saveRule: "
                      "Bad binding of Rule Type; sqlite error: " + to_string(rc));
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "prepare inserting is completed successfully";
#endif
    } else {
        throw IOError("AuditRulesHandlerSQLite::saveRule: "
                      "Run query; sqlite error: " + to_string(rc));
    }
}

const BaseAuditRule::AuditRuleType AuditRulesHandlerSQLite::getRule(
    TrustLineID trustLineID)
{
    string query = "SELECT rule_id FROM " + mTableName
                   + " WHERE trust_line_id = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("AuditRulesHandlerSQLite::getRule: "
                      "Bad binding of Trust Line ID; sqlite error: " + to_string(rc));
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        auto ruleId = (BaseAuditRule::AuditRuleType)sqlite3_column_int(stmt.get(), 0);
        return ruleId;
    } else {
        throw NotFoundError("AuditRulesHandlerSQLite::getRule: "
                            "There are no records with requested trust line id");
    }
}

void AuditRulesHandlerSQLite::removeAuditRules(
    TrustLineID trustLineID)
{
    string query = "DELETE FROM " + mTableName + " WHERE trust_line_id = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_int(stmt.get(), 1, trustLineID);
    if (rc != SQLITE_OK) {
        throw IOError("AuditRulesHandlerSQLite::removeAuditRules: "
                      "Bad binding of TrustLineID; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "deleting is completed successfully";
#endif
    } else {
        throw IOError("AuditRulesHandlerSQLite::removeAuditRules: "
                      "Run query; sqlite error: " + to_string(rc));
    }
}

LoggerStream AuditRulesHandlerSQLite::info() const
{
    return mLog.info(logHeader());
}

LoggerStream AuditRulesHandlerSQLite::warning() const
{
    return mLog.warning(logHeader());
}

const string AuditRulesHandlerSQLite::logHeader() const
{
    stringstream s;
    s << "[AuditRulesHandler]";
    return s.str();
}