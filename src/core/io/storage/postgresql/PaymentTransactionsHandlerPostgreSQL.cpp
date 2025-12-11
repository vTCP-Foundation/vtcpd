#include "PaymentTransactionsHandlerPostgreSQL.h"
#include <sstream>
#include "../../../common/time/TimeUtils.h"

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

PaymentTransactionsHandlerPostgreSQL::PaymentTransactionsHandlerPostgreSQL(
    PGconn *dbConnection,
    const string &tableName,
    Logger &logger) :
    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    if (!mDataBase) throw ValueError("PaymentTransactionsHandlerPostgreSQL: db null");
    if (mTableName.empty()) throw ValueError("PaymentTransactionsHandlerPostgreSQL: table name empty");

    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (uuid BYTEA NOT NULL, "
                   "maximal_claiming_block_number BYTEA NOT NULL, "
                   "observing_state INTEGER NOT NULL, "
                   "recording_time BIGINT NOT NULL, "
                   "payment_key_id BIGINT NOT NULL, "
                   "FOREIGN KEY(payment_key_id) REFERENCES payment_keys(id));";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase,res,"PaymentTx::create table");
    PQclear(res);

    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_uuid_idx ON " + mTableName + "(uuid);";
    res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase,res,"PaymentTx::uuid idx");
    PQclear(res);

    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_payment_key_id_idx ON " + mTableName + "(payment_key_id);";
    res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase,res,"PaymentTx::payment_key_id idx");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "PaymentTransactionsHandlerPostgreSQL init table=" << mTableName;
#endif
}

void PaymentTransactionsHandlerPostgreSQL::saveRecord(
    const TransactionUUID &transactionUUID,
    BlockNumber maximalClaimingBlockNumber)
{
    const string query = "INSERT INTO " + mTableName +
                         " (uuid, maximal_claiming_block_number, observing_state, recording_time, payment_key_id) VALUES ($1,$2,$3,$4,(SELECT id FROM payment_keys ORDER BY id DESC LIMIT 1));";
    const int kParams=4;
    const char *params[kParams];
    int lengths[kParams];
    int formats[kParams]={1,1,0,0};

    params[0]=reinterpret_cast<const char*>(transactionUUID.data); lengths[0]=TransactionUUID::kBytesSize;
    params[1]=reinterpret_cast<const char*>(&maximalClaimingBlockNumber); lengths[1]=sizeof(BlockNumber);
    string stateStr="0"; params[2]=stateStr.c_str(); lengths[2]=0;
    GEOEpochTimestamp ts = microsecondsSinceGEOEpoch(utc_now());
    string tsStr=to_string(ts); params[3]=tsStr.c_str(); lengths[3]=0;

    PGresult *res = PQexecParams(mDataBase, query.c_str(),kParams,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"saveRecord");
    PQclear(res);
}

void PaymentTransactionsHandlerPostgreSQL::updateTransactionState(
    const TransactionUUID &transactionUUID,
    int observingTransactionState)
{
    const string query="UPDATE " + mTableName + " SET observing_state=$1 WHERE uuid=$2;";
    const int kParams=2; const char *params[kParams]; int lengths[kParams]; int formats[kParams]={0,1};
    string stateStr=to_string(observingTransactionState); params[0]=stateStr.c_str(); lengths[0]=0;
    params[1]=reinterpret_cast<const char*>(transactionUUID.data); lengths[1]=TransactionUUID::kBytesSize;
    PGresult *res=PQexecParams(mDataBase,query.c_str(),kParams,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"updateState");
    if (PQcmdTuples(res)[0]=='0') { PQclear(res); throw ValueError("No rows affected"); }
    PQclear(res);
}

vector<pair<TransactionUUID, BlockNumber>> PaymentTransactionsHandlerPostgreSQL::transactionsWithUncertainObservingState()
{
    vector<pair<TransactionUUID, BlockNumber>> result;
    const string query="SELECT uuid, maximal_claiming_block_number FROM " + mTableName + " WHERE observing_state=0;";
    PGresult *res = PQexecParams(mDataBase, query.c_str(), 0, nullptr, nullptr, nullptr, nullptr, 1);  // Request binary result format
    checkTuples(mDataBase,res,"transactionsWithUncertainState");
    int rows=PQntuples(res);
    for (int i=0;i<rows;++i) {
        const unsigned char *uuidBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res,i,0));
        const unsigned char *blockBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res,i,1));
        TransactionUUID uuid(uuidBytes);
        BlockNumber bn; memcpy(&bn, blockBytes, sizeof(BlockNumber));
        result.emplace_back(uuid,bn);
    }
    PQclear(res);
    return result;
}

vector<pair<TransactionUUID, BlockNumber>> PaymentTransactionsHandlerPostgreSQL::transactionsForObserverMonitoring(
    BlockNumber minBlockNumber,
    uint32_t limit)
{
    vector<pair<TransactionUUID, BlockNumber>> result;
    const string query = "SELECT uuid, maximal_claiming_block_number FROM " + mTableName +
                         " WHERE maximal_claiming_block_number > $1 AND observing_state = 0 "
                         "ORDER BY maximal_claiming_block_number ASC LIMIT $2;";

    const int kParams = 2;
    const char *params[kParams];
    int lengths[kParams];
    int formats[kParams];

    params[0] = reinterpret_cast<const char*>(&minBlockNumber);
    lengths[0] = sizeof(BlockNumber);
    formats[0] = 1;

    string limitStr = to_string(limit);
    params[1] = limitStr.c_str();
    lengths[1] = 0;
    formats[1] = 0;

    PGresult *res = PQexecParams(
        mDataBase,
        query.c_str(),
        kParams,
        nullptr,
        params,
        lengths,
        formats,
        1);  // Request binary result format
    checkTuples(mDataBase, res, "transactionsForObserverMonitoring");

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        const unsigned char *uuidBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res, i, 0));
        const unsigned char *blockBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res, i, 1));
        TransactionUUID uuid(uuidBytes);
        BlockNumber maximalClaimingBlockNumber;
        memcpy(&maximalClaimingBlockNumber, blockBytes, sizeof(BlockNumber));
        result.emplace_back(uuid, maximalClaimingBlockNumber);
    }
    PQclear(res);

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Observer monitoring transactions retrieved: Count=" << result.size();
#endif

    return result;
}

bool PaymentTransactionsHandlerPostgreSQL::isTransactionPresent(
    const TransactionUUID &transactionUUID)
{
    const string query="SELECT COUNT(*) FROM " + mTableName + " WHERE uuid=$1;";
    const char *params[1]; int lengths[1]; int formats[1]={1};
    params[0]=reinterpret_cast<const char*>(transactionUUID.data); lengths[0]=TransactionUUID::kBytesSize;
    PGresult *res=PQexecParams(mDataBase,query.c_str(),1,nullptr,params,lengths,formats,0);
    checkTuples(mDataBase,res,"isTransactionPresent");
    bool present = atoi(PQgetvalue(res,0,0))>0;
    PQclear(res);
    return present;
}

void PaymentTransactionsHandlerPostgreSQL::deleteRecord(
    const TransactionUUID &transactionUUID)
{
    const string query="DELETE FROM " + mTableName + " WHERE uuid=$1;";
    const char *params[1]; int lengths[1]; int formats[1]={1};
    params[0]=reinterpret_cast<const char*>(transactionUUID.data); lengths[0]=TransactionUUID::kBytesSize;
    PGresult *res=PQexecParams(mDataBase,query.c_str(),1,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"deleteRecord");
    PQclear(res);
}

vector<TransactionUUID> PaymentTransactionsHandlerPostgreSQL::allTransactionsUUID()
{
    vector<TransactionUUID> result;
    const string query="SELECT uuid FROM " + mTableName + ";";
    PGresult *res = PQexecParams(mDataBase, query.c_str(), 0, nullptr, nullptr, nullptr, nullptr, 1);  // Request binary result format
    checkTuples(mDataBase,res,"allTransactionsUUID");
    int rows=PQntuples(res);
    for (int i=0;i<rows;++i) {
        const unsigned char *uuidBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res,i,0));
        result.emplace_back(uuidBytes);
    }
    PQclear(res);
    return result;
}

LoggerStream PaymentTransactionsHandlerPostgreSQL::info() const { return mLog.info(logHeader()); }
LoggerStream PaymentTransactionsHandlerPostgreSQL::warning() const { return mLog.warning(logHeader()); }
const string PaymentTransactionsHandlerPostgreSQL::logHeader() const { stringstream s; s << "[PaymentTransactionsHandlerPostgreSQL]"; return s.str(); } 
