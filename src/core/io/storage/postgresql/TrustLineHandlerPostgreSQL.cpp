#include "TrustLineHandlerPostgreSQL.h"
#include <sstream>
#include <cstring>

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
} // namespace

TrustLineHandlerPostgreSQL::TrustLineHandlerPostgreSQL(
    PGconn *dbConnection,
    const string &tableName,
    Logger &logger) :
    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    if (!mDataBase)
        throw ValueError("TrustLineHandlerPostgreSQL: db null");
    if (mTableName.empty())
        throw ValueError("TrustLineHandlerPostgreSQL: table name empty");

    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (id INT PRIMARY KEY, "
                   "state INT NOT NULL, "
                   "contractor_id INT NOT NULL, "
                   "equivalent INT NOT NULL, "
                   "is_contractor_gateway INT NOT NULL DEFAULT 0, "
                   "FOREIGN KEY(contractor_id) REFERENCES contractors(id) ON DELETE CASCADE ON UPDATE CASCADE);";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase, res, "TrustLineHandlerPG::create table");
    PQclear(res);

    // index on equivalent
    query = "CREATE INDEX IF NOT EXISTS " + mTableName +
            "_equivalent_idx ON " + mTableName + "(equivalent);";
    res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase, res, "TrustLineHandlerPG::equivalent idx");
    PQclear(res);

    // unique composite contractor_id + equivalent
    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName +
            "_contractor_id_equivalent_idx ON " + mTableName + "(contractor_id, equivalent);";
    res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase, res, "TrustLineHandlerPG::contractor/equivalent idx");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "TrustLineHandlerPostgreSQL initialized table=" << mTableName;
#endif
}

void TrustLineHandlerPostgreSQL::saveTrustLine(
    TrustLine::Shared trustLine,
    const SerializedEquivalent equivalent)
{
    if (!trustLine)
        throw ValueError("saveTrustLine: trustLine null");

    const string query = "INSERT INTO " + mTableName +
                         " (id, state, contractor_id, equivalent, is_contractor_gateway) "
                         "VALUES ($1,$2,$3,$4,$5) "
                         "ON CONFLICT (id) DO UPDATE SET state=EXCLUDED.state, contractor_id=EXCLUDED.contractor_id, equivalent=EXCLUDED.equivalent, is_contractor_gateway=EXCLUDED.is_contractor_gateway;";

    const int kParams = 5;
    const char *params[kParams];
    int lengths[kParams] = {0};
    int formats[kParams] = {0}; // text
    string idStr = to_string(trustLine->trustLineID()); params[0] = idStr.c_str();
    string stateStr = to_string(static_cast<int>(trustLine->state())); params[1] = stateStr.c_str();
    string contractorStr = to_string(trustLine->contractorID()); params[2] = contractorStr.c_str();
    string equivalentStr = to_string(equivalent); params[3] = equivalentStr.c_str();
    string gatewayStr = to_string(trustLine->isContractorGateway() ? 1 : 0); params[4] = gatewayStr.c_str();

    PGresult *res = PQexecParams(mDataBase, query.c_str(), kParams, nullptr, params, lengths, formats, 0);
    checkCmd(mDataBase, res, "saveTrustLine");
    PQclear(res);
}

void TrustLineHandlerPostgreSQL::updateTrustLineState(
    TrustLine::Shared trustLine,
    const SerializedEquivalent equivalent)
{
    if (!trustLine)
        throw ValueError("updateTrustLineState: trustLine null");

    const string query = "UPDATE " + mTableName +
                         " SET state=$1 WHERE id=$2 AND equivalent=$3 AND contractor_id=$4;";
    const int kParams = 4;
    const char *params[kParams];
    int lengths[kParams] = {0};
    int formats[kParams] = {0};
    string stateStr = to_string(static_cast<int>(trustLine->state())); params[0] = stateStr.c_str();
    string idStr = to_string(trustLine->trustLineID()); params[1] = idStr.c_str();
    string equivalentStr = to_string(equivalent); params[2] = equivalentStr.c_str();
    string contractorStr = to_string(trustLine->contractorID()); params[3] = contractorStr.c_str();

    PGresult *res = PQexecParams(mDataBase, query.c_str(), kParams, nullptr, params, lengths, formats, 0);
    checkCmd(mDataBase, res, "updateTrustLineState");

    int affected = atoi(PQcmdTuples(res));
    PQclear(res);
    if (affected == 0) {
        throw ValueError("No data were modified");
    }
}

void TrustLineHandlerPostgreSQL::updateTrustLineIsContractorGateway(
    TrustLine::Shared trustLine,
    const SerializedEquivalent equivalent)
{
    if (!trustLine)
        throw ValueError("updateTrustLineIsContractorGateway: trustLine null");

    const string query = "UPDATE " + mTableName +
                         " SET is_contractor_gateway=$1 WHERE id=$2 AND equivalent=$3 AND contractor_id=$4;";
    const int kParams = 4;
    const char *params[kParams];
    int lengths[kParams] = {0};
    int formats[kParams] = {0};
    string gatewayStr = to_string(trustLine->isContractorGateway() ? 1 : 0); params[0] = gatewayStr.c_str();
    string idStr = to_string(trustLine->trustLineID()); params[1] = idStr.c_str();
    string equivalentStr = to_string(equivalent); params[2] = equivalentStr.c_str();
    string contractorStr = to_string(trustLine->contractorID()); params[3] = contractorStr.c_str();

    PGresult *res = PQexecParams(mDataBase, query.c_str(), kParams, nullptr, params, lengths, formats, 0);
    checkCmd(mDataBase, res, "updateTrustLineIsContractorGateway");

    int affected = atoi(PQcmdTuples(res));
    PQclear(res);
    if (affected == 0) {
        throw ValueError("No data were modified");
    }
}

vector<TrustLine::Shared> TrustLineHandlerPostgreSQL::allTrustLinesByEquivalent(
    const SerializedEquivalent equivalent)
{
    vector<TrustLine::Shared> result;
    const string query = "SELECT id, state, contractor_id, is_contractor_gateway FROM " + mTableName + " WHERE equivalent=$1;";
    const char *params[1]; int lengths[1] = {0}; int formats[1] = {0};
    string equivalentStr = to_string(equivalent); params[0] = equivalentStr.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(), 1, nullptr, params, lengths, formats, 0);
    checkTuples(mDataBase, res, "allTrustLinesByEquivalent");
    int rows = PQntuples(res);
    result.reserve(rows);
    for (int i = 0; i < rows; ++i) {
        TrustLineID id = static_cast<TrustLineID>(atoi(PQgetvalue(res, i, 0)));
        TrustLine::TrustLineState state = static_cast<TrustLine::TrustLineState>(atoi(PQgetvalue(res, i, 1)));
        ContractorID contractorID = static_cast<ContractorID>(atoi(PQgetvalue(res, i, 2)));
        bool isGateway = atoi(PQgetvalue(res, i, 3)) != 0;
        try {
            result.push_back(make_shared<TrustLine>(id, contractorID, isGateway, state));
        } catch (...) {
            PQclear(res);
            throw Exception("TrustLineHandlerPostgreSQL::allTrustLinesByEquivalent: Unable to create TL");
        }
    }
    PQclear(res);
    return result;
}

void TrustLineHandlerPostgreSQL::deleteTrustLine(
    ContractorID contractorID,
    const SerializedEquivalent equivalent)
{
    const string query = "DELETE FROM " + mTableName + " WHERE contractor_id=$1 AND equivalent=$2;";
    const char *params[2]; int lengths[2] = {0}; int formats[2] = {0};
    string contractorStr = to_string(contractorID); params[0] = contractorStr.c_str();
    string equivalentStr = to_string(equivalent); params[1] = equivalentStr.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(), 2, nullptr, params, lengths, formats, 0);
    checkCmd(mDataBase, res, "deleteTrustLine");
    PQclear(res);
}

vector<SerializedEquivalent> TrustLineHandlerPostgreSQL::equivalents()
{
    vector<SerializedEquivalent> result;
    string query = "SELECT DISTINCT equivalent FROM " + mTableName + ";";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkTuples(mDataBase, res, "equivalents");
    int rows = PQntuples(res);
    result.reserve(rows);
    for (int i = 0; i < rows; ++i) {
        result.push_back(static_cast<SerializedEquivalent>(atoi(PQgetvalue(res, i, 0))));
    }
    PQclear(res);
    return result;
}

vector<TrustLineID> TrustLineHandlerPostgreSQL::allIDs()
{
    vector<TrustLineID> result;
    string query = "SELECT id FROM " + mTableName + ";";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkTuples(mDataBase, res, "allIDs");
    int rows = PQntuples(res);
    result.reserve(rows);
    for (int i = 0; i < rows; ++i) {
        result.push_back(static_cast<TrustLineID>(atoi(PQgetvalue(res, i, 0))));
    }
    PQclear(res);
    return result;
}

vector<TrustLine::Shared> TrustLineHandlerPostgreSQL::allTrustLinesByContractor(
    ContractorID contractorID)
{
    vector<TrustLine::Shared> result;
    const string query = "SELECT id, state, is_contractor_gateway FROM " + mTableName + " WHERE contractor_id=$1;";
    const char *params[1]; int lengths[1] = {0}; int formats[1] = {0};
    string contractorStr = to_string(contractorID); params[0] = contractorStr.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(), 1, nullptr, params, lengths, formats, 0);
    checkTuples(mDataBase, res, "allTrustLinesByContractor");
    int rows = PQntuples(res);
    result.reserve(rows);
    for (int i = 0; i < rows; ++i) {
        TrustLineID id = static_cast<TrustLineID>(atoi(PQgetvalue(res, i, 0)));
        TrustLine::TrustLineState state = static_cast<TrustLine::TrustLineState>(atoi(PQgetvalue(res, i, 1)));
        bool isGateway = atoi(PQgetvalue(res, i, 2)) != 0;
        try {
            result.push_back(make_shared<TrustLine>(id, contractorID, isGateway, state));
        } catch (...) {
            PQclear(res);
            throw Exception("TrustLineHandlerPostgreSQL::allTrustLinesByContractor: Unable to create TL");
        }
    }
    PQclear(res);
    return result;
}

LoggerStream TrustLineHandlerPostgreSQL::info() const { return mLog.info(logHeader()); }
LoggerStream TrustLineHandlerPostgreSQL::warning() const { return mLog.warning(logHeader()); }
const string TrustLineHandlerPostgreSQL::logHeader() const {
    stringstream s;
    s << "[TrustLineHandlerPostgreSQL]";
    return s.str();
} 