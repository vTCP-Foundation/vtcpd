#include "TrustLineHandlerSQLite.h"

TrustLineHandlerSQLite::TrustLineHandlerSQLite(
    sqlite3 *dbConnection,
    const string &tableName,
    Logger &logger) :

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   "(id INTEGER PRIMARY KEY, "
                   "state INTEGER NOT NULL, "
                   "contractor_id INTEGER NOT NULL, "
                   "equivalent INTEGER NOT NULL, "
                   "is_contractor_gateway INTEGER NOT NULL DEFAULT 0, "
                   "FOREIGN KEY(contractor_id) REFERENCES contractors(id) ON DELETE CASCADE ON UPDATE CASCADE);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("TrustLineHandlerSQLite::creating table: "
                      "Run query; sqlite error: " + to_string(rc));
    }

    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName
            + "_id_idx on " + mTableName + "(id);";
    SQLiteStatementRAII stmtUnique(mDataBase, query.c_str());
    rc = sqlite3_step(stmtUnique.get());
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("TrustLineHandlerSQLite::creating index for ID: "
                      "Run query; sqlite error: " + to_string(rc));
    }

    query = "CREATE INDEX IF NOT EXISTS " + mTableName
            + "_equivalent_idx on " + mTableName + "(equivalent);";
    SQLiteStatementRAII stmtEquivalent(mDataBase, query.c_str());
    rc = sqlite3_step(stmtEquivalent.get());
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("TrustLineHandlerSQLite::creating index for Equivalent: "
                      "Run query; sqlite error: " + to_string(rc));
    }

    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName
            + "_contractor_id_equivalent_idx on " + mTableName + "(contractor_id, equivalent);";
    SQLiteStatementRAII stmtUniqueEquivalent(mDataBase, query.c_str());
    rc = sqlite3_step(stmtUniqueEquivalent.get());
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("TrustLineHandlerSQLite::creating unique index for ContractorID and Equivalent: "
                      "Run query; sqlite error: " + to_string(rc));
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "TrustLineHandler initialized: table=" << mTableName;
#endif
}

vector<TrustLine::Shared> TrustLineHandlerSQLite::allTrustLinesByEquivalent(
    const SerializedEquivalent equivalent)
{
    string queryCount = "SELECT count(*) FROM " + mTableName + " WHERE equivalent = ?";
    SQLiteStatementRAII stmtCount(mDataBase, queryCount.c_str());
    int rc = sqlite3_bind_int(stmtCount.get(), 1, equivalent);
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::allTrustLinesByEquivalent: "
                      "Bad binding of Equivalent; sqlite error: " + to_string(rc));
    }
    sqlite3_step(stmtCount.get());
    auto rowCount = (uint32_t)sqlite3_column_int(stmtCount.get(), 0);
    vector<TrustLine::Shared> result;
    result.reserve(rowCount);

    string query = "SELECT id, state, contractor_id, is_contractor_gateway FROM "
                   + mTableName + " WHERE equivalent = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    rc = sqlite3_bind_int(stmt.get(), 1, equivalent);
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::allTrustLinesByEquivalent: "
                      "Bad binding of Equivalent; sqlite error: " + to_string(rc));
    }
    while (sqlite3_step(stmt.get()) == SQLITE_ROW ) {
        auto id = (TrustLineID)sqlite3_column_int(stmt.get(), 0);

        auto state = (TrustLine::TrustLineState)sqlite3_column_int(stmt.get(), 1);

        auto contractorID = (ContractorID)sqlite3_column_int(stmt.get(), 2);

        int32_t isContractorGateway = sqlite3_column_int(stmt.get(), 3);

        try {
            result.push_back(
                make_shared<TrustLine>(
                    id,
                    contractorID,
                    isContractorGateway != 0,
                    state));
        } catch (...) {
            throw Exception("TrustLinesManager::allTrustLinesByEquivalent. "
                            "Unable to get TLs from DB.");
        }
    }
    return result;
}

void TrustLineHandlerSQLite::deleteTrustLine(
    ContractorID contractorID,
    const SerializedEquivalent equivalent)
{
    string query = "DELETE FROM " + mTableName + " WHERE contractor_id = ? AND equivalent = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_int(stmt.get(), 1, contractorID);
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::deleteTrustLine: "
                      "Bad binding of ContractorID; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 2, equivalent);
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::deleteTrustLine: "
                      "Bad binding of Equivalent; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "deleting is completed successfully";
#endif
    } else {
        throw IOError("TrustLineHandlerSQLite::deleteTrustLine: "
                      "Run query; sqlite error: " + to_string(rc));
    }
}

void TrustLineHandlerSQLite::saveTrustLine(
    TrustLine::Shared trustLine,
    const SerializedEquivalent equivalent)
{
    string query = "INSERT INTO " + mTableName +
                   "(id, state, contractor_id, equivalent, is_contractor_gateway) "
                   "VALUES (?, ?, ?, ?, ?);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_int(stmt.get(), 1, trustLine->trustLineID());
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::saveTrustLine: "
                      "Bad binding of ID; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 2, (int)trustLine->state());
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::saveTrustLine: "
                      "Bad binding of State; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 3, trustLine->contractorID());
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::saveTrustLine: "
                      "Bad binding of ContractorID; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 4, equivalent);
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::saveTrustLine: "
                      "Bad binding of Equivalent; sqlite error: " + to_string(rc));
    }
    int32_t isContractorGateway = trustLine->isContractorGateway();
    rc = sqlite3_bind_int(stmt.get(), 5, isContractorGateway);
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::saveTrustLine: "
                      "Bad binding of IsContractorGateway; sqlite error: " + to_string(rc));
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "prepare inserting is completed successfully";
#endif
    } else {
        throw IOError("TrustLineHandlerSQLite::saveTrustLine: "
                      "Run query; sqlite error: " + to_string(rc));
    }
}

void TrustLineHandlerSQLite::updateTrustLineState(
    TrustLine::Shared trustLine,
    const SerializedEquivalent equivalent)
{
    string query = "UPDATE " + mTableName +
                   " SET state = ? "
                   "WHERE id = ? AND equivalent = ? AND contractor_id = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_int(stmt.get(), 1, (int)trustLine->state());
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::updateTrustLineState: "
                      "Bad binding of State; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 2, trustLine->trustLineID());
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::updateTrustLineState: "
                      "Bad binding of ID; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 3, equivalent);
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::updateTrustLineState: "
                      "Bad binding of Equivalent; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 4, trustLine->contractorID());
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::updateTrustLineState: "
                      "Bad binding of ContractorID; sqlite error: " + to_string(rc));
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "prepare updating is completed successfully";
#endif
    } else {
        throw IOError("TrustLineHandlerSQLite::updateTrustLineState: "
                      "Run query; sqlite error: " + to_string(rc));
    }

    if (sqlite3_changes(mDataBase) == 0) {
        throw ValueError("No data were modified");
    }
}

void TrustLineHandlerSQLite::updateTrustLineIsContractorGateway(
    TrustLine::Shared trustLine,
    const SerializedEquivalent equivalent)
{
    string query = "UPDATE " + mTableName +
                   " SET is_contractor_gateway = ? "
                   "WHERE id = ? AND equivalent = ? AND contractor_id = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    int32_t isContractorGateway = trustLine->isContractorGateway();
    int rc = sqlite3_bind_int(stmt.get(), 1, isContractorGateway);
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::updateTrustLineIsContractorGateway: "
                      "Bad binding of IsContractorGateway; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 2, trustLine->trustLineID());
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::updateTrustLineIsContractorGateway: "
                      "Bad binding of ID; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 3, equivalent);
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::updateTrustLineIsContractorGateway: "
                      "Bad binding of Equivalent; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 4, trustLine->contractorID());
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::updateTrustLineIsContractorGateway: "
                      "Bad binding of ContractorID; sqlite error: " + to_string(rc));
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "prepare updating is completed successfully";
#endif
    } else {
        throw IOError("TrustLineHandlerSQLite::updateTrustLineIsContractorGateway: "
                      "Run query; sqlite error: " + to_string(rc));
    }

    if (sqlite3_changes(mDataBase) == 0) {
        throw ValueError("No data were modified");
    }
}

vector<SerializedEquivalent> TrustLineHandlerSQLite::equivalents()
{
    string query = "SELECT DISTINCT equivalent FROM " + mTableName;
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    vector<SerializedEquivalent> result;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW ) {
        auto equivalent = (SerializedEquivalent)sqlite3_column_int(stmt.get(), 0);
        result.push_back(equivalent);
    }
    return result;
}

vector<TrustLineID> TrustLineHandlerSQLite::allIDs()
{
    string queryCount = "SELECT count(*) FROM " + mTableName;
    SQLiteStatementRAII stmtCount(mDataBase, queryCount.c_str());
    sqlite3_step(stmtCount.get());
    auto rowCount = (uint32_t)sqlite3_column_int(stmtCount.get(), 0);
    vector<TrustLineID> result;
    result.reserve(rowCount);

    string query = "SELECT id FROM " + mTableName;
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    while (sqlite3_step(stmt.get()) == SQLITE_ROW ) {
        result.push_back(
            (TrustLineID)sqlite3_column_int(stmt.get(), 0));
    }
    return result;
}

vector<TrustLine::Shared> TrustLineHandlerSQLite::allTrustLinesByContractor(
    ContractorID contractorID)
{
    string queryCount = "SELECT count(*) FROM " + mTableName + " WHERE contractor_id = ?";
    SQLiteStatementRAII stmtCount(mDataBase, queryCount.c_str());
    int rc = sqlite3_bind_int(stmtCount.get(), 1, contractorID);
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::allTrustLinesByContractor: "
                      "Bad binding of ContractorID; sqlite error: " + to_string(rc));
    }
    sqlite3_step(stmtCount.get());
    auto rowCount = (uint32_t)sqlite3_column_int(stmtCount.get(), 0);
    vector<TrustLine::Shared> result;
    result.reserve(rowCount);

    string query = "SELECT id, state, is_contractor_gateway FROM "
                   + mTableName + " WHERE contractor_id = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    rc = sqlite3_bind_int(stmt.get(), 1, contractorID);
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandlerSQLite::allTrustLinesByContractor: "
                      "Bad binding of ContractorID; sqlite error: " + to_string(rc));
    }
    while (sqlite3_step(stmt.get()) == SQLITE_ROW ) {
        auto id = (TrustLineID)sqlite3_column_int(stmt.get(), 0);

        auto state = (TrustLine::TrustLineState)sqlite3_column_int(stmt.get(), 1);

        int32_t isContractorGateway = sqlite3_column_int(stmt.get(), 2);

        try {
            result.push_back(
                make_shared<TrustLine>(
                    id,
                    contractorID,
                    isContractorGateway != 0,
                    state));
        } catch (...) {
            throw Exception("TrustLinesManager::allTrustLinesByContractor. "
                            "Unable to get TLs from DB.");
        }
    }
    return result;
}

LoggerStream TrustLineHandlerSQLite::info() const
{
    return mLog.info(logHeader());
}

LoggerStream TrustLineHandlerSQLite::warning() const
{
    return mLog.warning(logHeader());
}

const string TrustLineHandlerSQLite::logHeader() const
{
    stringstream s;
    s << "TrustLineHandler ";
    return s.str();
}
