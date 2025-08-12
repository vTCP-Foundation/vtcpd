#include "OwnKeysHandlerPostgreSQL.h"
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

OwnKeysHandlerPostgreSQL::OwnKeysHandlerPostgreSQL(
    PGconn *dbConnection,
    const string &tableName,
    Logger &logger) :
    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    if (!mDataBase) throw ValueError("OwnKeysHandlerPostgreSQL: db conn null");
    if (mTableName.empty()) throw ValueError("OwnKeysHandlerPostgreSQL: table name empty");

    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (hash BYTEA PRIMARY KEY, "
                   "trust_line_id INTEGER NOT NULL, "
                   "keys_set_sequence_number INTEGER NOT NULL, "
                   "public_key BYTEA NOT NULL, "
                   "private_key BYTEA NOT NULL, "
                   "FOREIGN KEY(trust_line_id) REFERENCES trust_lines(id) ON DELETE CASCADE ON UPDATE CASCADE);";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase,res,"OwnKeys::create table");
    PQclear(res);

    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName + "_hash_idx ON " + mTableName + "(hash);";
    res = PQexec(mDataBase, query.c_str()); checkCmd(mDataBase,res,"OwnKeys::hash idx"); PQclear(res);

    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_trust_line_id_idx ON " + mTableName + "(trust_line_id);";
    res = PQexec(mDataBase, query.c_str()); checkCmd(mDataBase,res,"OwnKeys::tl idx"); PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "OwnKeysHandlerPostgreSQL initialized table=" << mTableName;
#endif
}

void OwnKeysHandlerPostgreSQL::saveKey(
    const TrustLineID trustLineID,
    const KeyNumber keysSetSequenceNumber,
    const PublicKey::Shared publicKey,
    const PrivateKey *privateKey)
{
    if (!publicKey) throw ValueError("saveKey: publicKey null");
    if (privateKey==nullptr) throw ValueError("saveKey: privateKey null");

    auto keyHashShared = publicKey->hash();

    const string query = "INSERT INTO " + mTableName +
                         "(hash, trust_line_id, keys_set_sequence_number, public_key, private_key) "
                         "VALUES ($1,$2,$3,$4,$5) ON CONFLICT (hash) DO UPDATE SET "
                         "trust_line_id=EXCLUDED.trust_line_id, "
                         "keys_set_sequence_number=EXCLUDED.keys_set_sequence_number, "
                         "public_key=EXCLUDED.public_key, "
                         "private_key=EXCLUDED.private_key;";

    const int kParams=5;
    const char *params[kParams];
    int lengths[kParams];
    int formats[kParams]={1,0,0,1,1};

    params[0]=reinterpret_cast<const char*>(keyHashShared->data()); lengths[0]=KeyHash::kBytesSize;
    string tlIdStr=to_string(trustLineID); params[1]=tlIdStr.c_str(); lengths[1]=0;
    string seqStr=to_string(keysSetSequenceNumber); params[2]=seqStr.c_str(); lengths[2]=0;
    params[3]=reinterpret_cast<const char*>(publicKey->data()); lengths[3]=publicKey->keySize();

    auto privateKeyData = privateKey->serialize();
    auto guard = privateKeyData.unlockAndInitGuard();
    params[4]=reinterpret_cast<const char*>(guard.address()); lengths[4]=PrivateKey::privateKeySize();

    PGresult *res = PQexecParams(mDataBase, query.c_str(), kParams,nullptr,params,lengths,formats,0);
    if (PQresultStatus(res)!=PGRES_COMMAND_OK) {
        string err = PQerrorMessage(mDataBase); PQclear(res); throw IOError("saveKey: "+err);
    }
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Key saved TL=" << trustLineID << " seq=" << keysSetSequenceNumber;
#endif
}

const KeyNumber OwnKeysHandlerPostgreSQL::maxKeySetSequenceNumber(
    const TrustLineID trustLineID)
{
    const string query="SELECT MAX(keys_set_sequence_number) FROM " + mTableName + " WHERE trust_line_id=$1;";
    const char *params[1]; int lengths[1]={0}; int formats[1]={0};
    string tlStr=to_string(trustLineID); params[0]=tlStr.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),1,nullptr,params,lengths,formats,0);
    checkTuples(mDataBase,res,"maxSeq");
    if (PQntuples(res)==0 || PQgetvalue(res,0,0) == nullptr || strlen(PQgetvalue(res,0,0)) == 0) { PQclear(res); throw NotFoundError("No keys"); }
    KeyNumber seq = static_cast<KeyNumber>(atoi(PQgetvalue(res,0,0)));
    PQclear(res);
    return seq;
}

std::unique_ptr<PrivateKey> OwnKeysHandlerPostgreSQL::getPrivateKey(
    const TrustLineID trustLineID)
{
    const string query="SELECT private_key FROM " + mTableName + " WHERE trust_line_id=$1 LIMIT 1;";
    const char *params[1]; int lengths[1]={0}; int formats[1]={0};
    string tlStr=to_string(trustLineID); params[0]=tlStr.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),1,nullptr,params,lengths,formats,1);
    checkTuples(mDataBase,res,"getPrivateKey");
    if (PQntuples(res)==0) { PQclear(res); throw NotFoundError("No key found"); }
    
    // Safely copy private key data before clearing the result
    const unsigned char *privBytesConst = reinterpret_cast<const unsigned char*>(PQgetvalue(res,0,0));
    
    // Create a safe copy of the private key data
    BytesShared privBuf = tryMalloc(PrivateKey::privateKeySize());
    memcpy(privBuf.get(), privBytesConst, PrivateKey::privateKeySize());
    
    PQclear(res);
    
    // Create PrivateKey from the copied data
    auto privKey = make_unique<PrivateKey>(privBuf.get());
    return privKey;
}

void OwnKeysHandlerPostgreSQL::invalidateKey(
    const TrustLineID trustLineID,
    const Signature::Shared signature)
{
    if (!signature) throw ValueError("invalidateKey: signature null");
    // In new single-key architecture, invalidation means deletion
    const string query="DELETE FROM " + mTableName + " WHERE trust_line_id=$1;";
    const char *params[1]; int lengths[1]={0}; int formats[1]={0};
    string tlStr=to_string(trustLineID); params[0]=tlStr.c_str(); lengths[0]=0;
    PGresult *res = PQexecParams(mDataBase, query.c_str(),1,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"invalidateKey");
    if (PQcmdTuples(res)[0]=='0') { PQclear(res); throw ValueError("No rows affected"); }
    PQclear(res);
}

void OwnKeysHandlerPostgreSQL::invalidateKeyByHash(
    const TrustLineID trustLineID,
    const KeyHash::Shared keyHash,
    const Signature::Shared signature)
{
    if (!keyHash || !signature) throw ValueError("invalidateKeyByHash: null param");
    // In new single-key architecture, invalidation means deletion
    const string query="DELETE FROM " + mTableName + " WHERE trust_line_id=$1 AND hash=$2;";
    const char *params[2]; int lengths[2]={0,1}; int formats[2]={0,1};
    string tlStr=to_string(trustLineID); params[0]=tlStr.c_str(); lengths[0]=0;
    params[1]=reinterpret_cast<const char*>(keyHash->data()); lengths[1]=KeyHash::kBytesSize;
    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"invalidateKeyByHash");
    if (PQcmdTuples(res)[0]=='0') { PQclear(res); throw ValueError("No rows affected"); }
    PQclear(res);
}

const PublicKey::Shared OwnKeysHandlerPostgreSQL::getPublicKey(
    const TrustLineID trustLineID)
{
    const string query="SELECT public_key FROM " + mTableName + " WHERE trust_line_id=$1 LIMIT 1;";
    const char *params[1]; int lengths[1]={0}; int formats[1]={0};
    string tlStr=to_string(trustLineID);
    params[0]=tlStr.c_str();
    PGresult *res=PQexecParams(mDataBase,query.c_str(),1,nullptr,params,lengths,formats,1);
    checkTuples(mDataBase,res,"getPublicKey");
    if (PQntuples(res)==0) { PQclear(res); throw NotFoundError("Public key not found"); }
    
    // Safely copy public key data before clearing the result
    const unsigned char *pubBytesConst = reinterpret_cast<const unsigned char*>(PQgetvalue(res,0,0));
    BytesShared pubBuf = tryMalloc(PublicKey::keySize());
    memcpy(pubBuf.get(), pubBytesConst, PublicKey::keySize());
    
    PQclear(res);
    
    auto pub=make_shared<PublicKey>(pubBuf.get());
    return pub;
}

const PublicKey::Shared OwnKeysHandlerPostgreSQL::getPublicKeyByHash(
    const TrustLineID trustLineID,
    const KeyHash::Shared keyHash)
{
    if (!keyHash) throw ValueError("getPublicKeyByHash: keyHash null");
    const string query="SELECT public_key FROM " + mTableName + " WHERE trust_line_id=$1 AND hash=$2 LIMIT 1;";
    const char *params[2]; int lengths[2]; int formats[2]={0,1};
    string tlStr=to_string(trustLineID); params[0]=tlStr.c_str(); lengths[0]=0;
    params[1]=reinterpret_cast<const char*>(keyHash->data()); lengths[1]=KeyHash::kBytesSize;
    PGresult *res=PQexecParams(mDataBase,query.c_str(),2,nullptr,params,lengths,formats,1);
    checkTuples(mDataBase,res,"getPublicKeyByHash");
    if (PQntuples(res)==0) { PQclear(res); throw NotFoundError("Public key not found"); }
    
    // Safely copy public key data before clearing the result
    const unsigned char *pubBytesConst = reinterpret_cast<const unsigned char*>(PQgetvalue(res,0,0));
    BytesShared pubBuf = tryMalloc(PublicKey::keySize());
    memcpy(pubBuf.get(), pubBytesConst, PublicKey::keySize());
    
    PQclear(res);
    
    auto pub=make_shared<PublicKey>(pubBuf.get());
    return pub;
}

const KeyHash::Shared OwnKeysHandlerPostgreSQL::getPublicKeyHash(
    const TrustLineID trustLineID)
{
    const string query="SELECT hash FROM " + mTableName + " WHERE trust_line_id=$1 LIMIT 1;";
    const char *params[1]; int lengths[1]={0}; int formats[1]={0};
    string tlStr=to_string(trustLineID);
    params[0]=tlStr.c_str();
    PGresult *res=PQexecParams(mDataBase,query.c_str(),1,nullptr,params,lengths,formats,1);
    checkTuples(mDataBase,res,"getPublicKeyHash");
    if (PQntuples(res)==0) { PQclear(res); throw NotFoundError("Key hash not found"); }
    
    // Safely copy hash data before clearing the result
    const unsigned char *hashBytesConst = reinterpret_cast<const unsigned char*>(PQgetvalue(res,0,0));
    BytesShared hashBuf = tryMalloc(KeyHash::kBytesSize);
    memcpy(hashBuf.get(), hashBytesConst, KeyHash::kBytesSize);
    
    PQclear(res);
    
    auto hash=make_shared<KeyHash>(hashBuf.get());
    return hash;
}


bool OwnKeysHandlerPostgreSQL::hasKey(
    const TrustLineID trustLineID)
{
    const string query="SELECT count(*) FROM " + mTableName + " WHERE trust_line_id=$1;";
    const char *params[1]; int lengths[1]={0}; int formats[1]={0}; string tlStr=to_string(trustLineID); params[0]=tlStr.c_str();
    PGresult *res=PQexecParams(mDataBase,query.c_str(),1,nullptr,params,lengths,formats,0);
    checkTuples(mDataBase,res,"hasKey");
    if (PQntuples(res)==0) { PQclear(res); throw IOError("hasKey: no result"); }
    int cnt = atoi(PQgetvalue(res,0,0));
    PQclear(res);
    return cnt > 0;
}


vector<PublicKey::Shared> OwnKeysHandlerPostgreSQL::publicKeysBySetNumber(
    const TrustLineID trustLineID,
    const KeyNumber keysSetSequenceNumber) const
{
    vector<PublicKey::Shared> result;
    const string query="SELECT public_key FROM " + mTableName + " WHERE trust_line_id=$1 AND keys_set_sequence_number=$2 ORDER BY hash;";
    const char *params[2]; int lengths[2]={0,0}; int formats[2]={0,0}; string tlStr=to_string(trustLineID); string seqStr=to_string(keysSetSequenceNumber);
    params[0]=tlStr.c_str(); params[1]=seqStr.c_str();
    PGresult *res=PQexecParams(mDataBase,query.c_str(),2,nullptr,params,lengths,formats,1);
    checkTuples(mDataBase,res,"publicKeysBySetNumber");
    int rows=PQntuples(res); result.reserve(rows);
    for (int i=0;i<rows;++i) {
        // Safely copy public key data before creating the object
        const unsigned char *pubBytesConst = reinterpret_cast<const unsigned char*>(PQgetvalue(res,i,0));
        BytesShared pubBuf = tryMalloc(PublicKey::keySize());
        memcpy(pubBuf.get(), pubBytesConst, PublicKey::keySize());
        
        auto pub=make_shared<PublicKey>(pubBuf.get());
        result.push_back(pub);
    }
    PQclear(res);
    return result;
}

void OwnKeysHandlerPostgreSQL::deleteKeysByTrustLineID(
    const TrustLineID trustLineID)
{
    const string query="DELETE FROM " + mTableName + " WHERE trust_line_id=$1;";
    const char *params[1]; int lengths[1]={0}; int formats[1]={0}; string tlStr=to_string(trustLineID); params[0]=tlStr.c_str();
    PGresult *res=PQexecParams(mDataBase,query.c_str(),1,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"deleteKeysByTL");
    PQclear(res);
}

void OwnKeysHandlerPostgreSQL::deleteKeyByHashExceptSequenceNumber(
    KeyHash::Shared keyHash,
    const KeyNumber keysSetSequenceNumber)
{
    if (!keyHash) throw ValueError("deleteKeyByHashExceptSeq: keyHash null");
    const string query="DELETE FROM " + mTableName + " WHERE hash=$1 AND keys_set_sequence_number!=$2;";
    const char *params[2]; int lengths[2]; int formats[2]={1,0};
    params[0]=reinterpret_cast<const char*>(keyHash->data()); lengths[0]=KeyHash::kBytesSize;
    string seqStr=to_string(keysSetSequenceNumber); params[1]=seqStr.c_str(); lengths[1]=0;
    PGresult *res=PQexecParams(mDataBase,query.c_str(),2,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"deleteKeyByHashExceptSeq");
    PQclear(res);
}

vector<KeyHash::Shared> OwnKeysHandlerPostgreSQL::publicKeyHashesLessThanSetNumber(
    const TrustLineID trustLineID,
    const KeyNumber keysSetSequenceNumber) const
{
    vector<KeyHash::Shared> result;
    const string query="SELECT hash FROM " + mTableName + " WHERE trust_line_id=$1 AND keys_set_sequence_number<$2;";
    const char *params[2]; int lengths[2]={0,0}; int formats[2]={0,0}; string tlStr=to_string(trustLineID); string seqStr=to_string(keysSetSequenceNumber);
    params[0]=tlStr.c_str(); params[1]=seqStr.c_str();
    PGresult *res=PQexecParams(mDataBase,query.c_str(),2,nullptr,params,lengths,formats,1);
    checkTuples(mDataBase,res,"publicKeyHashesLessThanSetNumber");
    int rows=PQntuples(res); result.reserve(rows);
    for (int i=0;i<rows;++i) {
        // Safely copy hash data before creating the object
        const unsigned char *hashBytesConst = reinterpret_cast<const unsigned char*>(PQgetvalue(res,i,0));
        BytesShared hashBuf = tryMalloc(KeyHash::kBytesSize);
        memcpy(hashBuf.get(), hashBytesConst, KeyHash::kBytesSize);
        
        auto hash=make_shared<KeyHash>(hashBuf.get());
        result.push_back(hash);
    }
    PQclear(res);
    return result;
}

LoggerStream OwnKeysHandlerPostgreSQL::info() const { return mLog.info(logHeader()); }
LoggerStream OwnKeysHandlerPostgreSQL::warning() const { return mLog.warning(logHeader()); }
const string OwnKeysHandlerPostgreSQL::logHeader() const { stringstream s; s << "[OwnKeysHandlerPostgreSQL]"; return s.str(); } 