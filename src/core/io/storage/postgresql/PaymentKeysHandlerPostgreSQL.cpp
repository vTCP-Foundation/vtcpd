#include "PaymentKeysHandlerPostgreSQL.h"
#include "../../../common/exceptions/ValueError.h"
#include <sstream>
#include <vector>
#include <cstdlib>

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

PaymentKeysHandlerPostgreSQL::PaymentKeysHandlerPostgreSQL(
    PGconn *dbConnection,
    const string &tableName,
    Logger &logger) :
    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    if (!mDataBase) throw ValueError("PaymentKeysHandlerPostgreSQL: db connection null");
    if (mTableName.empty()) throw ValueError("PaymentKeysHandlerPostgreSQL: table name empty");

    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (id BIGSERIAL PRIMARY KEY, "
                   "public_key BYTEA NOT NULL, "
                   "private_key BYTEA NOT NULL);";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase,res,"PaymentKeys::create table");
    PQclear(res);

    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_id_idx ON " + mTableName + "(id);";
    res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase,res,"PaymentKeys::index uuid");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "PaymentKeysHandlerPostgreSQL initialized table=" << mTableName;
#endif
}

void PaymentKeysHandlerPostgreSQL::saveOwnKey(
    const PublicKey::Shared publicKey,
    const PrivateKey *privateKey)
{
    if (!publicKey || privateKey==nullptr) {
        throw ValueError("saveOwnKey: null key");
    }
    const string query = "INSERT INTO " + mTableName +
                         "(public_key, private_key) VALUES ($1,$2);";
    const int kParams = 2;
    const char *params[kParams];
    int lengths[kParams];
    int formats[kParams] = {1,1};

    params[0] = reinterpret_cast<const char*>(publicKey->data()); lengths[0]=publicKey->keySize();

    auto privateKeyData = privateKey->serialize();
    auto guard = privateKeyData.unlockAndInitGuard();
    params[1] = reinterpret_cast<const char*>(guard.address()); lengths[1]=privateKey->privateKeySize();

    PGresult *res = PQexecParams(mDataBase, query.c_str(), kParams, nullptr, params, lengths, formats, 0);
    checkCmd(mDataBase,res,"saveOwnKey");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Own payment key saved";
#endif
}

PrivateKey* PaymentKeysHandlerPostgreSQL::getOwnPrivateKey()
{
    const string query = "SELECT private_key FROM " + mTableName + " ORDER BY id DESC LIMIT 1;";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkTuples(mDataBase,res,"getOwnPrivateKey");
    if (PQntuples(res)==0) { PQclear(res); throw NotFoundError("Private key not found"); }
    
    // Get raw data from PostgreSQL
    int dataLength = PQgetlength(res, 0, 0);
    const unsigned char *rawData = reinterpret_cast<const unsigned char*>(PQgetvalue(res,0,0));
    
    // Check if data is in hex format (starts with \x and has 2*keySize + 2 length)
    if (dataLength == static_cast<int>(PrivateKey::privateKeySize() * 2 + 2) && 
        rawData[0] == '\\' && rawData[1] == 'x') {
        // Convert from hex format
        std::vector<byte_t> binaryData(PrivateKey::privateKeySize());
        for (size_t i = 0; i < PrivateKey::privateKeySize(); ++i) {
            char hex[3] = {static_cast<char>(rawData[2 + i*2]), static_cast<char>(rawData[3 + i*2]), '\0'};
            binaryData[i] = static_cast<byte_t>(strtoul(hex, nullptr, 16));
        }
        auto privKey = new PrivateKey(binaryData.data());
        PQclear(res);
        return privKey;
    } else if (dataLength == static_cast<int>(PrivateKey::privateKeySize())) {
        // Data is already in binary format
        auto privKey = new PrivateKey(reinterpret_cast<byte_t*>(const_cast<unsigned char*>(rawData)));
        PQclear(res);
        return privKey;
    } else {
        PQclear(res);
        throw IOError("Invalid private key data length: expected " + 
                     to_string(PrivateKey::privateKeySize()) + " or " + 
                     to_string(PrivateKey::privateKeySize() * 2 + 2) + 
                     ", got " + to_string(dataLength));
    }
}

void PaymentKeysHandlerPostgreSQL::deleteKeyByID(
    const uint64_t id)
{
    const string query = "DELETE FROM " + mTableName + " WHERE id=$1;";
    const int kParams=1; const char *params[kParams]; int lengths[kParams]; int formats[kParams]={0};
    string idStr = to_string(id); params[0]=idStr.c_str(); lengths[0]=0;
    PGresult *res = PQexecParams(mDataBase, query.c_str(),kParams,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"deleteKeyByUUID");
    PQclear(res);
}

bool PaymentKeysHandlerPostgreSQL::hasAnyKeys()
{
    const string query = "SELECT COUNT(*) FROM " + mTableName + ";";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkTuples(mDataBase,res,"hasAnyKeys");
    bool present = atoi(PQgetvalue(res,0,0))>0;
    PQclear(res);
    return present;
}

PublicKey::Shared PaymentKeysHandlerPostgreSQL::getOwnPublicKey()
{
    const string query = "SELECT public_key FROM " + mTableName + " ORDER BY id DESC LIMIT 1;";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkTuples(mDataBase,res,"getOwnPublicKey");
    if (PQntuples(res)==0) { PQclear(res); throw NotFoundError("Public key not found"); }
    
    // Get raw data from PostgreSQL
    int dataLength = PQgetlength(res, 0, 0);
    const unsigned char *rawData = reinterpret_cast<const unsigned char*>(PQgetvalue(res,0,0));
    
    // Check if data is in hex format (starts with \x and has 2*keySize + 2 length)
    if (dataLength == static_cast<int>(PublicKey::keySize() * 2 + 2) && 
        rawData[0] == '\\' && rawData[1] == 'x') {
        // Convert from hex format
        std::vector<byte_t> binaryData(PublicKey::keySize());
        for (size_t i = 0; i < PublicKey::keySize(); ++i) {
            char hex[3] = {static_cast<char>(rawData[2 + i*2]), static_cast<char>(rawData[3 + i*2]), '\0'};
            binaryData[i] = static_cast<byte_t>(strtoul(hex, nullptr, 16));
        }
        auto pubKey = make_shared<PublicKey>(binaryData.data());
        PQclear(res);
        return pubKey;
    } else if (dataLength == static_cast<int>(PublicKey::keySize())) {
        // Data is already in binary format
        auto pubKey = make_shared<PublicKey>(reinterpret_cast<byte_t*>(const_cast<unsigned char*>(rawData)));
        PQclear(res);
        return pubKey;
    } else {
        PQclear(res);
        throw IOError("Invalid public key data length: expected " + 
                     to_string(PublicKey::keySize()) + " or " + 
                     to_string(PublicKey::keySize() * 2 + 2) + 
                     ", got " + to_string(dataLength));
    }
}

uint64_t PaymentKeysHandlerPostgreSQL::latestKeyID()
{
    const string query = "SELECT id FROM " + mTableName + " ORDER BY id DESC LIMIT 1;";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkTuples(mDataBase,res,"latestKeyID");
    if (PQntuples(res)==0) { PQclear(res); throw NotFoundError("No keys found"); }
    uint64_t id = static_cast<uint64_t>(strtoull(PQgetvalue(res,0,0), nullptr, 10));
    PQclear(res);
    return id;
}

LoggerStream PaymentKeysHandlerPostgreSQL::info() const { return mLog.info(logHeader()); }
LoggerStream PaymentKeysHandlerPostgreSQL::warning() const { return mLog.warning(logHeader()); }
const string PaymentKeysHandlerPostgreSQL::logHeader() const { stringstream s; s << "[PaymentKeysHandlerPostgreSQL]"; return s.str(); } 