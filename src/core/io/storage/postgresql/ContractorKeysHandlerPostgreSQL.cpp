#include "ContractorKeysHandlerPostgreSQL.h"
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

ContractorKeysHandlerPostgreSQL::ContractorKeysHandlerPostgreSQL(
    PGconn *dbConnection,
    const string &tableName,
    Logger &logger) :
    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    if (!mDataBase) {
        throw ValueError("ContractorKeysHandlerPostgreSQL: db connection null");
    }
    if (mTableName.empty()) {
        throw ValueError("ContractorKeysHandlerPostgreSQL: table name empty");
    }
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (hash BYTEA PRIMARY KEY, "
                   "trust_line_id INTEGER NOT NULL, "
                   "keys_set_sequence_number INTEGER NOT NULL, "
                   "public_key BYTEA NOT NULL, "
                   "number INTEGER NOT NULL, "
                   "is_valid INTEGER NOT NULL DEFAULT 1, "
                   "FOREIGN KEY(trust_line_id) REFERENCES trust_lines(id) ON DELETE CASCADE ON UPDATE CASCADE);";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase, res, "ContractorKeysHandlerPostgreSQL::creating table");
    PQclear(res);

    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName + "_hash_idx on " + mTableName + "(hash);";
    res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase, res, "ContractorKeysHandlerPostgreSQL::index hash");
    PQclear(res);

    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_trust_line_id_idx on " + mTableName + "(trust_line_id);";
    res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase, res, "ContractorKeysHandlerPostgreSQL::index trust_line_id");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "ContractorKeysHandlerPostgreSQL initialized: table=" << mTableName;
#endif
}

void ContractorKeysHandlerPostgreSQL::saveKey(
    const TrustLineID trustLineID,
    const KeyNumber keysSetSequenceNumber,
    const PublicKey::Shared publicKey,
    const KeyNumber number)
{
    if (!publicKey) {
        throw ValueError("saveKey: publicKey null");
    }

    auto keyHashShared = publicKey->hash();

    const string query = "INSERT INTO " + mTableName +
                         "(hash, trust_line_id, keys_set_sequence_number, public_key, number) "
                         "VALUES ($1,$2,$3,$4,$5);";

    const int kParams = 5;
    const char *params[kParams];
    int lengths[kParams];
    int formats[kParams] = {1,0,0,1,0};

    params[0] = reinterpret_cast<const char *>(keyHashShared->data());
    lengths[0] = KeyHash::kBytesSize;

    string tlIdStr = to_string(trustLineID);
    params[1] = tlIdStr.c_str(); lengths[1]=0;

    string seqStr = to_string(keysSetSequenceNumber);
    params[2] = seqStr.c_str(); lengths[2]=0;

    params[3] = reinterpret_cast<const char *>(publicKey->data());
    lengths[3] = publicKey->keySize();

    string numStr = to_string(number);
    params[4] = numStr.c_str(); lengths[4]=0;

    PGresult *res = PQexecParams(mDataBase, query.c_str(), kParams, nullptr, params, lengths, formats, 0);
    checkCmd(mDataBase, res, "ContractorKeysHandlerPostgreSQL::saveKey");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Key saved TL=" << trustLineID << " num=" << number;
#endif
}

const KeyNumber ContractorKeysHandlerPostgreSQL::maxKeySetSequenceNumber(
    const TrustLineID trustLineID)
{
    const string query = "SELECT MAX(keys_set_sequence_number) FROM " + mTableName + " WHERE trust_line_id=$1;";
    const char *params[1]; int lengths[1]={0}; int formats[1]={0};
    string tlIdStr = to_string(trustLineID); params[0]=tlIdStr.c_str();

    PGresult *res = PQexecParams(mDataBase, query.c_str(), 1, nullptr, params, lengths, formats, 0);
    checkTuples(mDataBase, res, "maxKeySetSequenceNumber");
    if (PQntuples(res)==0 || PQgetvalue(res,0,0)==nullptr) {
        PQclear(res);
        throw NotFoundError("No keys for TL");
    }
    KeyNumber seq = static_cast<KeyNumber>(atoi(PQgetvalue(res,0,0)));
    PQclear(res);
    return seq;
}

void ContractorKeysHandlerPostgreSQL::invalidKey(
    const TrustLineID trustLineID,
    const KeyNumber number)
{
    const string query = "UPDATE " + mTableName + " SET is_valid=0 WHERE trust_line_id=$1 AND number=$2;";
    const char *params[2]; int lengths[2]={0,0}; int formats[2]={0,0};
    string tlIdStr=to_string(trustLineID); string numStr=to_string(number);
    params[0]=tlIdStr.c_str(); params[1]=numStr.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"invalidKey");
    if (PQcmdTuples(res)[0]=='0') { PQclear(res); throw ValueError("No rows affected"); }
    PQclear(res);
}

void ContractorKeysHandlerPostgreSQL::invalidateKeyByHash(
    const TrustLineID trustLineID,
    const KeyHash::Shared keyHash)
{
    if (!keyHash) throw ValueError("invalidateKeyByHash: keyHash null");
    const string query = "UPDATE " + mTableName + " SET is_valid=0 WHERE trust_line_id=$1 AND hash=$2;";
    const char *params[2]; int lengths[2]; int formats[2]={0,1};
    string tlIdStr=to_string(trustLineID);
    params[0]=tlIdStr.c_str(); lengths[0]=0;
    params[1]=reinterpret_cast<const char *>(keyHash->data()); lengths[1]=KeyHash::kBytesSize;
    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"invalidateKeyByHash");
    if (PQcmdTuples(res)[0]=='0') { PQclear(res); throw ValueError("No rows affected"); }
    PQclear(res);
}

PublicKey::Shared ContractorKeysHandlerPostgreSQL::keyByNumber(
    const TrustLineID trustLineID,
    const KeyNumber keyNumber)
{
    const string query = "SELECT public_key FROM " + mTableName + " WHERE trust_line_id=$1 AND number=$2 LIMIT 1;";
    const char *params[2]; int lengths[2]={0,0}; int formats[2]={0,0};
    string tlIdStr=to_string(trustLineID); string numStr=to_string(keyNumber);
    params[0]=tlIdStr.c_str(); params[1]=numStr.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,params,lengths,formats,1);
    checkTuples(mDataBase,res,"keyByNumber");
    if (PQntuples(res)==0) { PQclear(res); throw NotFoundError("Public key not found"); }
    auto pub = make_shared<PublicKey>(reinterpret_cast<const unsigned char *>(PQgetvalue(res,0,0)));
    PQclear(res);
    return pub;
}

PublicKey::Shared ContractorKeysHandlerPostgreSQL::keyByHash(
    const TrustLineID trustLineID,
    const KeyHash::Shared keyHash)
{
    if (!keyHash) throw ValueError("keyByHash: keyHash null");
    const string query = "SELECT public_key FROM " + mTableName + " WHERE trust_line_id=$1 AND hash=$2 LIMIT 1;";
    const char *params[2]; int lengths[2]; int formats[2]={0,1};
    string tlIdStr=to_string(trustLineID);
    params[0]=tlIdStr.c_str(); lengths[0]=0;
    params[1]=reinterpret_cast<const char *>(keyHash->data()); lengths[1]=KeyHash::kBytesSize;
    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,params,lengths,formats,1);
    checkTuples(mDataBase,res,"keyByHash");
    if (PQntuples(res)==0) { PQclear(res); throw NotFoundError("Public key not found"); }
    auto pub = make_shared<PublicKey>(reinterpret_cast<const unsigned char *>(PQgetvalue(res,0,0)));
    PQclear(res);
    return pub;
}

const KeyHash::Shared ContractorKeysHandlerPostgreSQL::keyHashByNumber(
    const TrustLineID trustLineID,
    const KeyNumber keyNumber)
{
    const string query = "SELECT hash FROM " + mTableName + " WHERE trust_line_id=$1 AND number=$2 LIMIT 1;";
    const char *params[2]; int lengths[2]={0,0}; int formats[2]={0,0};
    string tlIdStr=to_string(trustLineID); string numStr=to_string(keyNumber);
    params[0]=tlIdStr.c_str(); params[1]=numStr.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,params,lengths,formats,1);
    checkTuples(mDataBase,res,"keyHashByNumber");
    if (PQntuples(res)==0) { PQclear(res); throw NotFoundError("Key hash not found"); }
    auto hash = make_shared<KeyHash>(reinterpret_cast<const unsigned char *>(PQgetvalue(res,0,0)));
    PQclear(res);
    return hash;
}

KeysCount ContractorKeysHandlerPostgreSQL::availableKeysCnt(
    const TrustLineID trustLineID)
{
    const string query = "SELECT count(*) FROM " + mTableName + " WHERE trust_line_id=$1 AND is_valid=1;";
    const char *p[1]; int l[1]={0}; int f[1]={0}; string s=to_string(trustLineID); p[0]=s.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),1,nullptr,p,l,f,0);
    checkTuples(mDataBase,res,"availableKeysCnt");
    KeysCount c = static_cast<KeysCount>(atoi(PQgetvalue(res,0,0)));
    PQclear(res);
    return c;
}

KeysCount ContractorKeysHandlerPostgreSQL::sequenceKeysCnt(
    const TrustLineID trustLineID,
    KeyNumber keysSetSequenceNumber)
{
    const string query = "SELECT count(*) FROM " + mTableName + " WHERE trust_line_id=$1 AND keys_set_sequence_number=$2;";
    const char *p[2]; int l[2]={0,0}; int f[2]={0,0}; string tl=to_string(trustLineID); string seq=to_string(keysSetSequenceNumber); p[0]=tl.c_str(); p[1]=seq.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,p,l,f,0);
    checkTuples(mDataBase,res,"sequenceKeysCnt");
    KeysCount c=static_cast<KeysCount>(atoi(PQgetvalue(res,0,0)));
    PQclear(res);
    return c;
}

void ContractorKeysHandlerPostgreSQL::removeUnusedKeys(
    const TrustLineID trustLineID)
{
    const string query = "DELETE FROM " + mTableName + " WHERE trust_line_id=$1 AND is_valid=1;";
    const char *p[1]; int l[1]={0}; int f[1]={0}; string tl=to_string(trustLineID); p[0]=tl.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),1,nullptr,p,l,f,0);
    checkCmd(mDataBase,res,"removeUnusedKeys");
    PQclear(res);
}

vector<PublicKey::Shared> ContractorKeysHandlerPostgreSQL::publicKeysBySetNumber(
    const TrustLineID trustLineID,
    const KeyNumber keysSetSequenceNumber) const
{
    vector<PublicKey::Shared> result;
    const string query = "SELECT public_key FROM " + mTableName + " WHERE trust_line_id=$1 AND keys_set_sequence_number=$2 ORDER BY number;";
    const char *p[2]; int l[2]={0,0}; int f[2]={0,0}; string tl=to_string(trustLineID); string seq=to_string(keysSetSequenceNumber); p[0]=tl.c_str(); p[1]=seq.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,p,l,f,1);
    checkTuples(mDataBase,res,"publicKeysBySetNumber");
    int rows = PQntuples(res);
    result.reserve(rows);
    for(int i=0;i<rows;++i) {
        result.push_back(make_shared<PublicKey>(reinterpret_cast<const unsigned char *>(PQgetvalue(res,i,0))));
    }
    PQclear(res);
    return result;
}

void ContractorKeysHandlerPostgreSQL::deleteKeysByTrustLineID(
    const TrustLineID trustLineID)
{
    const string query = "DELETE FROM " + mTableName + " WHERE trust_line_id=$1;";
    const char *p[1]; int l[1]={0}; int f[1]={0}; string tl=to_string(trustLineID); p[0]=tl.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),1,nullptr,p,l,f,0);
    checkCmd(mDataBase,res,"deleteKeysByTrustLineID");
    PQclear(res);
}

void ContractorKeysHandlerPostgreSQL::deleteKeyByHashExceptSequenceNumber(
    KeyHash::Shared keyHash,
    const KeyNumber keysSetSequenceNumber)
{
    if (!keyHash) throw ValueError("deleteKeyByHashExceptSequenceNumber: keyHash null");
    const string query = "DELETE FROM " + mTableName + " WHERE hash=$1 AND keys_set_sequence_number<>$2;";
    const char *p[2]; int l[2]; int f[2]={1,0};
    p[0]=reinterpret_cast<const char *>(keyHash->data()); l[0]=KeyHash::kBytesSize;
    string seq=to_string(keysSetSequenceNumber); p[1]=seq.c_str(); l[1]=0;
    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,p,l,f,0);
    checkCmd(mDataBase,res,"deleteKeyByHashExceptSequenceNumber");
    PQclear(res);
}

vector<KeyHash::Shared> ContractorKeysHandlerPostgreSQL::publicKeyHashesLessThanSetNumber(
    const TrustLineID trustLineID,
    const KeyNumber keysSetSequenceNumber) const
{
    vector<KeyHash::Shared> result;
    const string query = "SELECT hash FROM " + mTableName + " WHERE trust_line_id=$1 AND keys_set_sequence_number<$2;";
    const char *p[2]; int l[2]={0,0}; int f[2]={0,1}; string tl=to_string(trustLineID); string seq=to_string(keysSetSequenceNumber); p[0]=tl.c_str(); p[1]=seq.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,p,l,f,1);
    checkTuples(mDataBase,res,"publicKeyHashesLessThanSetNumber");
    int rows=PQntuples(res);
    result.reserve(rows);
    for(int i=0;i<rows;++i) {
        result.push_back(make_shared<KeyHash>(reinterpret_cast<const unsigned char *>(PQgetvalue(res,i,0))));
    }
    PQclear(res);
    return result;
}

LoggerStream ContractorKeysHandlerPostgreSQL::info() const { return mLog.info(logHeader()); }
LoggerStream ContractorKeysHandlerPostgreSQL::warning() const { return mLog.warning(logHeader()); }
const string ContractorKeysHandlerPostgreSQL::logHeader() const { stringstream s; s << "[ContractorKeysHandlerPostgreSQL]"; return s.str(); } 