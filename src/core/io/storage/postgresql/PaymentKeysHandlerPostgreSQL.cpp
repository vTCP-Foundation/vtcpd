#include "PaymentKeysHandlerPostgreSQL.h"
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
                   " (transaction_uuid BYTEA NOT NULL, "
                   "public_key BYTEA NOT NULL, "
                   "private_key BYTEA NOT NULL);";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase,res,"PaymentKeys::create table");
    PQclear(res);

    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_transaction_uuid_idx ON " + mTableName + "(transaction_uuid);";
    res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase,res,"PaymentKeys::index uuid");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "PaymentKeysHandlerPostgreSQL initialized table=" << mTableName;
#endif
}

void PaymentKeysHandlerPostgreSQL::saveOwnKey(
    const TransactionUUID &transactionUUID,
    const PublicKey::Shared publicKey,
    const PrivateKey *privateKey)
{
    if (!publicKey || privateKey==nullptr) {
        throw ValueError("saveOwnKey: null key");
    }
    const string query = "INSERT INTO " + mTableName +
                         "(transaction_uuid, public_key, private_key) VALUES ($1,$2,$3);";
    const int kParams = 3;
    const char *params[kParams];
    int lengths[kParams];
    int formats[kParams] = {1,1,1};

    params[0] = reinterpret_cast<const char*>(transactionUUID.data); lengths[0]=TransactionUUID::kBytesSize;
    params[1] = reinterpret_cast<const char*>(publicKey->data()); lengths[1]=publicKey->keySize();

    BytesShared privBuf = tryMalloc(privateKey->keySize());
    {
        auto g = privateKey->data()->unlockAndInitGuard();
        memcpy(privBuf.get(), g.address(), privateKey->keySize());
    }
    params[2] = reinterpret_cast<const char*>(privBuf.get()); lengths[2]=privateKey->keySize();

    PGresult *res = PQexecParams(mDataBase, query.c_str(), kParams, nullptr, params, lengths, formats, 0);
    checkCmd(mDataBase,res,"saveOwnKey");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Own payment key saved";
#endif
}

PrivateKey* PaymentKeysHandlerPostgreSQL::getOwnPrivateKey(
    const TransactionUUID &transactionUUID)
{
    const string query = "SELECT private_key FROM " + mTableName + " WHERE transaction_uuid=$1 LIMIT 1;";
    const char *params[1]; int lengths[1]; int formats[1]={1};
    params[0]=reinterpret_cast<const char*>(transactionUUID.data); lengths[0]=TransactionUUID::kBytesSize;
    PGresult *res = PQexecParams(mDataBase, query.c_str(),1,nullptr,params,lengths,formats,1);
    checkTuples(mDataBase,res,"getOwnPrivateKey");
    if (PQntuples(res)==0) { PQclear(res); throw NotFoundError("Private key not found"); }
    const unsigned char *privBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res,0,0));
    auto privKey = new PrivateKey(privBytes);
    PQclear(res);
    return privKey;
}

void PaymentKeysHandlerPostgreSQL::deleteKeyByTransactionUUID(
    const TransactionUUID &transactionUUID)
{
    const string query = "DELETE FROM " + mTableName + " WHERE transaction_uuid=$1;";
    const char *params[1]; int lengths[1]; int formats[1]={1};
    params[0]=reinterpret_cast<const char*>(transactionUUID.data); lengths[0]=TransactionUUID::kBytesSize;
    PGresult *res = PQexecParams(mDataBase, query.c_str(),1,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"deleteKeyByUUID");
    PQclear(res);
}

vector<TransactionUUID> PaymentKeysHandlerPostgreSQL::allTransactionUUIDs()
{
    vector<TransactionUUID> result;
    const string query = "SELECT transaction_uuid FROM " + mTableName + ";";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkTuples(mDataBase,res,"allTransactionUUIDs");
    int rows = PQntuples(res);
    for (int i=0;i<rows;++i) {
        const unsigned char *uuidBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res,i,0));
        result.emplace_back(uuidBytes);
    }
    PQclear(res);
    return result;
}

LoggerStream PaymentKeysHandlerPostgreSQL::info() const { return mLog.info(logHeader()); }
LoggerStream PaymentKeysHandlerPostgreSQL::warning() const { return mLog.warning(logHeader()); }
const string PaymentKeysHandlerPostgreSQL::logHeader() const { stringstream s; s << "[PaymentKeysHandlerPostgreSQL]"; return s.str(); } 