#include "ContractorsHandlerPostgreSQL.h"
#include <sstream>
#include <arpa/inet.h>

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

ContractorsHandlerPostgreSQL::ContractorsHandlerPostgreSQL(
    PGconn *dbConnection,
    const string &tableName,
    Logger &logger) :
    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    if (!mDataBase) throw ValueError("ContractorsHandlerPostgreSQL: db null");
    if (mTableName.empty()) throw ValueError("ContractorsHandlerPostgreSQL: table name empty");

    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   "(id INTEGER PRIMARY KEY, "
                   "id_on_contractor_side INTEGER, "
                   "crypto_key BYTEA NOT NULL, "
                   "is_confirmed INTEGER NOT NULL DEFAULT 0);";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase, res, "ContractorsHandlerPostgreSQL::create table");
    PQclear(res);

    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName + "_id_idx on " + mTableName + "(id);";
    res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase, res, "ContractorsHandlerPostgreSQL::index id");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "ContractorsHandlerPostgreSQL init: table=" << mTableName;
#endif
}

void ContractorsHandlerPostgreSQL::saveContractor(
    Contractor::Shared contractor)
{
    const string query = "INSERT INTO " + mTableName + "(id, crypto_key) VALUES ($1,$2);";
    vector<byte_t> cryptoBlob;
    auto cryptoKey = contractor->cryptoKey();
    if (cryptoKey) cryptoKey->serialize(cryptoBlob);

    const char *p[2]; int l[2]; int f[2]={0,1};
    string idStr = to_string(contractor->getID()); p[0]=idStr.c_str(); l[0]=0;
    p[1]=reinterpret_cast<const char *>(cryptoBlob.data()); l[1]=cryptoBlob.size();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,p,l,f,0);
    checkCmd(mDataBase,res,"saveContractor");
    PQclear(res);
}

void ContractorsHandlerPostgreSQL::saveContractorFull(
    Contractor::Shared contractor)
{
    const string query = "INSERT INTO " + mTableName + "(id,id_on_contractor_side,crypto_key,is_confirmed) VALUES ($1,$2,$3,1);";
    vector<byte_t> cryptoBlob; if (contractor->cryptoKey()) contractor->cryptoKey()->serialize(cryptoBlob);
    const char *p[3]; int l[3]; int f[3]={0,0,1};
    string idStr=to_string(contractor->getID()); p[0]=idStr.c_str(); l[0]=0;
    string idSideStr=to_string(contractor->ownIdOnContractorSide()); p[1]=idSideStr.c_str(); l[1]=0;
    p[2]=reinterpret_cast<const char *>(cryptoBlob.data()); l[2]=cryptoBlob.size();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),3,nullptr,p,l,f,0);
    checkCmd(mDataBase,res,"saveContractorFull");
    PQclear(res);
}

void ContractorsHandlerPostgreSQL::saveConfirmationInfo(
    Contractor::Shared contractor)
{
    const string query = "UPDATE " + mTableName + " SET id_on_contractor_side=$1, crypto_key=$2, is_confirmed=1 WHERE id=$3;";
    vector<byte_t> cryptoBlob; if (contractor->cryptoKey()) contractor->cryptoKey()->serialize(cryptoBlob);
    const char *p[3]; int l[3]; int f[3]={0,1,0};
    string idSideStr=to_string(contractor->ownIdOnContractorSide()); p[0]=idSideStr.c_str(); l[0]=0;
    p[1]=reinterpret_cast<const char *>(cryptoBlob.data()); l[1]=cryptoBlob.size();
    string idStr=to_string(contractor->getID()); p[2]=idStr.c_str(); l[2]=0;
    PGresult *res = PQexecParams(mDataBase, query.c_str(),3,nullptr,p,l,f,0);
    checkCmd(mDataBase,res,"saveConfirmationInfo");
    if (PQcmdTuples(res)[0]=='0') { PQclear(res); throw ValueError("No data modified"); }
    PQclear(res);
}

void ContractorsHandlerPostgreSQL::updateCryptoKey(
    Contractor::Shared contractor)
{
    const string query = "UPDATE " + mTableName + " SET crypto_key=$1, is_confirmed=1 WHERE id=$2;";
    vector<byte_t> cryptoBlob; if (contractor->cryptoKey()) contractor->cryptoKey()->serialize(cryptoBlob);
    const char *p[2]; int l[2]; int f[2]={1,0};
    p[0]=reinterpret_cast<const char *>(cryptoBlob.data()); l[0]=cryptoBlob.size();
    string idStr=to_string(contractor->getID()); p[1]=idStr.c_str(); l[1]=0;
    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,p,l,f,0);
    checkCmd(mDataBase,res,"updateCryptoKey");
    if (PQcmdTuples(res)[0]=='0') { PQclear(res); throw ValueError("No data modified"); }
    PQclear(res);
}

void ContractorsHandlerPostgreSQL::updateChannelIdOnContractorSide(
    Contractor::Shared contractor)
{
    const string query = "UPDATE " + mTableName + " SET id_on_contractor_side=$1 WHERE id=$2;";
    const char *p[2]; int l[2]={0,0}; int f[2]={0,0};
    string idSideStr=to_string(contractor->ownIdOnContractorSide()); p[0]=idSideStr.c_str();
    string idStr=to_string(contractor->getID()); p[1]=idStr.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,p,l,f,0);
    checkCmd(mDataBase,res,"updateChannelIdOnContractorSide");
    if (PQcmdTuples(res)[0]=='0') { PQclear(res); throw ValueError("No data modified"); }
    PQclear(res);
}

vector<Contractor::Shared> ContractorsHandlerPostgreSQL::allContractors()
{
    vector<Contractor::Shared> result;
    const string query = "SELECT id,id_on_contractor_side,crypto_key,is_confirmed FROM " + mTableName + ";";
    PGresult *res = PQexecParams(mDataBase, query.c_str(), 0, nullptr, nullptr, nullptr, nullptr, 1);
    checkTuples(mDataBase,res,"allContractors");
    int rows = PQntuples(res);
    result.reserve(rows);
    for(int i=0;i<rows;++i) {
        // Read binary integer data (network byte order)
        auto id = static_cast<ContractorID>(ntohl(*reinterpret_cast<const uint32_t*>(PQgetvalue(res,i,0))));
        
        // Handle NULL value for id_on_contractor_side
        ContractorID idSide = 0;
        if (!PQgetisnull(res, i, 1)) {
            idSide = static_cast<ContractorID>(ntohl(*reinterpret_cast<const uint32_t*>(PQgetvalue(res,i,1))));
        }
        
        // Read binary BYTEA data
        const unsigned char *blob = reinterpret_cast<const unsigned char *>(PQgetvalue(res,i,2));
        int blobSize = PQgetlength(res,i,2);
        vector<byte_t> crypto(blob, blob+blobSize);
        auto key = make_shared<MsgEncryptor::KeyTrio>(crypto);
        
        // Read binary integer data for confirmed flag
        bool confirmed = ntohl(*reinterpret_cast<const uint32_t*>(PQgetvalue(res,i,3))) == 1;
        
        try {
            result.push_back(make_shared<Contractor>(id,idSide,key,confirmed));
        } catch(...) {
            PQclear(res);
            throw Exception("allContractors: unable to create contractor");
        }
    }
    PQclear(res);
    return result;
}

vector<ContractorID> ContractorsHandlerPostgreSQL::allIDs()
{
    vector<ContractorID> result;
    const string query="SELECT id FROM " + mTableName + ";";
    PGresult *res = PQexecParams(mDataBase, query.c_str(), 0, nullptr, nullptr, nullptr, nullptr, 1);
    checkTuples(mDataBase,res,"allIDs");
    int rows=PQntuples(res); result.reserve(rows);
    for(int i=0;i<rows;++i) {
        result.push_back(static_cast<ContractorID>(ntohl(*reinterpret_cast<const uint32_t*>(PQgetvalue(res,i,0)))));
    }
    PQclear(res);
    return result;
}

void ContractorsHandlerPostgreSQL::removeContractor(
    ContractorID contractorID)
{
    const string query = "DELETE FROM " + mTableName + " WHERE id=$1;";
    const char *p[1]; int l[1]={0}; int f[1]={0}; string idStr=to_string(contractorID); p[0]=idStr.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),1,nullptr,p,l,f,0);
    checkCmd(mDataBase,res,"removeContractor");
    if (PQcmdTuples(res)[0]=='0') { PQclear(res); throw ValueError("No data deleted"); }
    PQclear(res);
}

LoggerStream ContractorsHandlerPostgreSQL::info() const { return mLog.info(logHeader()); }
LoggerStream ContractorsHandlerPostgreSQL::warning() const { return mLog.warning(logHeader()); }
const string ContractorsHandlerPostgreSQL::logHeader() const { stringstream s; s << "[ContractorsHandlerPostgreSQL]"; return s.str(); } 