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

    // Create main table
    // effective_claiming_block_number stores the extended claiming deadline that includes
    // the dispute grace period. This value equals maximal_claiming_block_number +
    // disputeGracePeriodBlocksCount and is used for observer monitoring queries.
    // BlockNumber columns are stored as BIGINT (not BYTEA) to enable correct numeric comparisons.
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (uuid BYTEA NOT NULL, "
                   "maximal_claiming_block_number BIGINT NOT NULL, "
                   "effective_claiming_block_number BIGINT NOT NULL, "
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
    BlockNumber maximalClaimingBlockNumber,
    BlockNumber effectiveClaimingBlockNumber)
{
    // Insert payment transaction record with both claiming block numbers:
    // - maximal_claiming_block_number: original claiming deadline
    // - effective_claiming_block_number: extended deadline including dispute grace period
    const string query = "INSERT INTO " + mTableName +
                         " (uuid, maximal_claiming_block_number, effective_claiming_block_number, "
                         "observing_state, recording_time, payment_key_id) "
                         "VALUES ($1,$2,$3,$4,$5,(SELECT id FROM payment_keys ORDER BY id DESC LIMIT 1));";
    const int kParams=5;
    const char *params[kParams];
    int lengths[kParams];
    int formats[kParams]={1,0,0,0,0};

    params[0]=reinterpret_cast<const char*>(transactionUUID.data); lengths[0]=TransactionUUID::kBytesSize;
    // Store BlockNumber as text (BIGINT requires text format for parameterized queries)
    string maxBlockStr=to_string(maximalClaimingBlockNumber); params[1]=maxBlockStr.c_str(); lengths[1]=0;
    // Bind effective claiming block number (includes dispute grace period)
    string effBlockStr=to_string(effectiveClaimingBlockNumber); params[2]=effBlockStr.c_str(); lengths[2]=0;
    string stateStr="0"; params[3]=stateStr.c_str(); lengths[3]=0;
    GEOEpochTimestamp ts = microsecondsSinceGEOEpoch(utc_now());
    string tsStr=to_string(ts); params[4]=tsStr.c_str(); lengths[4]=0;

    PGresult *res = PQexecParams(mDataBase, query.c_str(),kParams,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"saveRecord");
    PQclear(res);
}

void PaymentTransactionsHandlerPostgreSQL::updateTransactionState(
    const TransactionUUID &transactionUUID,
    PaymentObservingState observingState)
{
    const string query="UPDATE " + mTableName + " SET observing_state=$1 WHERE uuid=$2;";
    const int kParams=2; const char *params[kParams]; int lengths[kParams]; int formats[kParams]={0,1};
    string stateStr=to_string(static_cast<int>(observingState)); params[0]=stateStr.c_str(); lengths[0]=0;
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
    // Use binary format (resultFormat=1) for proper data retrieval
    PGresult *res = PQexecParams(mDataBase, query.c_str(), 0, nullptr, nullptr, nullptr, nullptr, 1);
    checkTuples(mDataBase,res,"transactionsWithUncertainState");
    int rows=PQntuples(res);
    for (int i=0;i<rows;++i) {
        const unsigned char *uuidBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res,i,0));
        TransactionUUID uuid(uuidBytes);
        // BIGINT in binary format is 8 bytes big-endian, convert to host byte order
        const unsigned char *blockBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res,i,1));
        BlockNumber bn = 0;
        for (int j = 0; j < 8; ++j) {
            bn = (bn << 8) | blockBytes[j];
        }
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
    // Fetch transactions still within effective claiming window (including dispute grace period)
    // with uncertain observing state. The effective_claiming_block_number already includes
    // the dispute grace period, so we compare directly against the current block number.
    const string query = "SELECT uuid, maximal_claiming_block_number FROM " + mTableName +
                         " WHERE effective_claiming_block_number > $1 AND observing_state = 1 "
                         "ORDER BY maximal_claiming_block_number ASC LIMIT $2;";

    const int kParams = 2;
    const char *params[kParams];
    int lengths[kParams];
    int formats[kParams];

    // Pass minBlockNumber as text for BIGINT comparison
    string minBlockStr = to_string(minBlockNumber);
    params[0] = minBlockStr.c_str();
    lengths[0] = 0;
    formats[0] = 0;

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
        TransactionUUID uuid(uuidBytes);
        // BIGINT in binary format is 8 bytes big-endian, convert to host byte order
        const unsigned char *blockBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res, i, 1));
        BlockNumber maximalClaimingBlockNumber = 0;
        for (int j = 0; j < 8; ++j) {
            maximalClaimingBlockNumber = (maximalClaimingBlockNumber << 8) | blockBytes[j];
        }
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

BlockNumber PaymentTransactionsHandlerPostgreSQL::effectiveClaimingBlockNumber(
    const TransactionUUID &transactionUUID)
{
    const string query = "SELECT effective_claiming_block_number FROM " + mTableName + " WHERE uuid=$1 LIMIT 1;";
    const char *params[1]; int lengths[1]; int formats[1]={1};
    params[0]=reinterpret_cast<const char*>(transactionUUID.data); lengths[0]=TransactionUUID::kBytesSize;
    PGresult *res=PQexecParams(mDataBase, query.c_str(), 1, nullptr, params, lengths, formats, 1);
    checkTuples(mDataBase, res, "effectiveClaimingBlockNumber");

    if (PQntuples(res) == 0) {
        PQclear(res);
        throw NotFoundError("PaymentTransactionsHandlerPostgreSQL::effectiveClaimingBlockNumber: Transaction not found.");
    }

    const unsigned char *blockBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res, 0, 0));
    BlockNumber effectiveClaimingBlockNumber = 0;
    for (int i = 0; i < 8; ++i) {
        effectiveClaimingBlockNumber = (effectiveClaimingBlockNumber << 8) | blockBytes[i];
    }
    PQclear(res);
    return effectiveClaimingBlockNumber;
}

BlockNumber PaymentTransactionsHandlerPostgreSQL::maximalClaimingBlockNumber(
    const TransactionUUID &transactionUUID)
{
    const string query = "SELECT maximal_claiming_block_number FROM " + mTableName + " WHERE uuid=$1 LIMIT 1;";
    const char *params[1];
    int lengths[1];
    int formats[1] = {1};
    params[0] = reinterpret_cast<const char*>(transactionUUID.data);
    lengths[0] = TransactionUUID::kBytesSize;
    PGresult *res = PQexecParams(mDataBase, query.c_str(), 1, nullptr, params, lengths, formats, 1);
    checkTuples(mDataBase, res, "maximalClaimingBlockNumber");

    if (PQntuples(res) == 0) {
        PQclear(res);
        throw NotFoundError("PaymentTransactionsHandlerPostgreSQL::maximalClaimingBlockNumber: Transaction not found.");
    }

    const unsigned char *blockBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res, 0, 0));
    BlockNumber maximalClaimingBlockNumber = 0;
    for (int i = 0; i < 8; ++i) {
        maximalClaimingBlockNumber = (maximalClaimingBlockNumber << 8) | blockBytes[i];
    }
    PQclear(res);
    return maximalClaimingBlockNumber;
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
