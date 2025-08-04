#include "FeaturesHandlerPostgreSQL.h"
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

FeaturesHandlerPostgreSQL::FeaturesHandlerPostgreSQL(
    PGconn *dbConnection,
    const string &tableName,
    Logger &logger) :
    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    if (!mDataBase) throw ValueError("FeaturesHandlerPostgreSQL: db null");
    if (mTableName.empty()) throw ValueError("FeaturesHandlerPostgreSQL: table name empty");

    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (feature_name TEXT PRIMARY KEY, "
                   "feature_length INTEGER NOT NULL, "
                   "feature_value TEXT NOT NULL);";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase, res, "FeaturesHandlerPostgreSQL::create table");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "FeaturesHandlerPostgreSQL init table=" << mTableName;
#endif
}

void FeaturesHandlerPostgreSQL::saveFeature(
    const string &featureName,
    const string &featureValue)
{
    if (featureName.empty()) throw ValueError("saveFeature: featureName empty");
    const string query = "INSERT INTO " + mTableName +
                         " (feature_name, feature_length, feature_value) "
                         "VALUES ($1,$2,$3) "
                         "ON CONFLICT(feature_name) DO UPDATE SET feature_length=EXCLUDED.feature_length, feature_value=EXCLUDED.feature_value;";

    const char *p[3]; int l[3]={0}; int f[3]={0};
    p[0]=featureName.c_str();
    string lenStr = to_string(featureValue.size()); p[1]=lenStr.c_str();
    p[2]=featureValue.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),3,nullptr,p,l,f,0);
    checkCmd(mDataBase,res,"saveFeature");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Feature saved name=" << featureName;
#endif
}

string FeaturesHandlerPostgreSQL::getFeature(
    const string &featureName)
{
    if (featureName.empty()) throw ValueError("getFeature: featureName empty");
    const string query = "SELECT feature_value FROM " + mTableName + " WHERE feature_name=$1;";
    const char *p[1]; int l[1]={0}; int f[1]={0}; p[0]=featureName.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),1,nullptr,p,l,f,0);
    checkTuples(mDataBase,res,"getFeature");
    if (PQntuples(res)==0) { PQclear(res); throw NotFoundError("Feature not found"); }
    string value = PQgetvalue(res,0,0);
    PQclear(res);
    return value;
}

LoggerStream FeaturesHandlerPostgreSQL::info() const { return mLog.info(logHeader()); }
LoggerStream FeaturesHandlerPostgreSQL::warning() const { return mLog.warning(logHeader()); }
const string FeaturesHandlerPostgreSQL::logHeader() const { stringstream s; s << "[FeaturesHandlerPostgreSQL]"; return s.str(); } 