#include "TransactionsHandlerPostgreSQL.h"
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

TransactionsHandlerPostgreSQL::TransactionsHandlerPostgreSQL(
    PGconn *dbConnection,
    const string &tableName,
    Logger &logger) :
    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    if (!mDataBase) throw ValueError("TransactionsHandlerPostgreSQL: db null");
    if (mTableName.empty()) throw ValueError("TransactionsHandlerPostgreSQL: table name empty");

    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (transaction_uuid BYTEA NOT NULL, "
                   "transaction_body BYTEA NOT NULL, "
                   "transaction_bytes_count INT NOT NULL);";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase,res,"Transactions::create table");
    PQclear(res);

    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName + "_transaction_uuid_idx ON " + mTableName + "(transaction_uuid);";
    res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase,res,"Transactions::uuid idx");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "TransactionsHandlerPostgreSQL initialized table=" << mTableName;
#endif
}

void TransactionsHandlerPostgreSQL::saveRecord(
    const TransactionUUID &transactionUUID,
    BytesShared transaction,
    size_t transactionBytesCount)
{
    if (!transaction) throw ValueError("saveRecord: transaction null");
    if (transactionBytesCount==0) throw ValueError("saveRecord: bytesCount zero");

    const string query = "INSERT INTO " + mTableName +
                         " (transaction_uuid, transaction_body, transaction_bytes_count) VALUES ($1,$2,$3) "
                         "ON CONFLICT (transaction_uuid) DO UPDATE SET transaction_body=EXCLUDED.transaction_body, transaction_bytes_count=EXCLUDED.transaction_bytes_count;";
    const int kParams=3;
    const char *params[kParams]; int lengths[kParams]; int formats[kParams]={1,1,0};
    params[0]=reinterpret_cast<const char*>(transactionUUID.data); lengths[0]=TransactionUUID::kBytesSize;
    params[1]=reinterpret_cast<const char*>(transaction.get()); lengths[1]=transactionBytesCount;
    string cntStr=to_string(transactionBytesCount); params[2]=cntStr.c_str(); lengths[2]=0;
    PGresult *res=PQexecParams(mDataBase,query.c_str(),kParams,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"saveRecord");
    PQclear(res);
}

BytesShared TransactionsHandlerPostgreSQL::getTransaction(
    const TransactionUUID &transactionUUID)
{
    const string query="SELECT transaction_body, transaction_bytes_count FROM " + mTableName + " WHERE transaction_uuid=$1 LIMIT 1;";
    const char *params[1]; int lengths[1]; int formats[1]={1};
    params[0]=reinterpret_cast<const char*>(transactionUUID.data); lengths[0]=TransactionUUID::kBytesSize;
    
    // Use binary format for the result to get raw bytes
    PGresult *res=PQexecParams(mDataBase,query.c_str(),1,nullptr,params,lengths,formats,1);
    checkTuples(mDataBase,res,"getTransaction");
    if (PQntuples(res)==0) { PQclear(res); throw NotFoundError("Transaction not found"); }
    
    // Get bytes count from the integer field (4 bytes in network byte order)
    int bytesCountFieldLength = PQgetlength(res, 0, 1);
    if (bytesCountFieldLength != 4) { PQclear(res); throw IOError("Invalid bytes count field length"); }
    
    // Convert from network byte order to host byte order
    uint32_t bytesCnt32 = ntohl(*reinterpret_cast<const uint32_t*>(PQgetvalue(res, 0, 1)));
    size_t bytesCnt = static_cast<size_t>(bytesCnt32);
    
    if (bytesCnt==0) { PQclear(res); throw IOError("Invalid bytes count"); }
    BytesShared data = tryMalloc(bytesCnt);
    memcpy(data.get(), PQgetvalue(res,0,0), bytesCnt);
    PQclear(res);
    return data;
}

bool TransactionsHandlerPostgreSQL::isTransactionSerialized(
    const TransactionUUID &transactionUUID)
{
    const string query="SELECT 1 FROM " + mTableName + " WHERE transaction_uuid=$1 LIMIT 1;";
    const char *params[1]; int lengths[1]; int formats[1]={1};
    params[0]=reinterpret_cast<const char*>(transactionUUID.data); lengths[0]=TransactionUUID::kBytesSize;
    PGresult *res=PQexecParams(mDataBase,query.c_str(),1,nullptr,params,lengths,formats,0);
    checkTuples(mDataBase,res,"isSerialized");
    bool present = PQntuples(res)>0;
    PQclear(res);
    return present;
}

void TransactionsHandlerPostgreSQL::deleteRecordIfExists(
    const TransactionUUID &transactionUUID)
{
    const string query="DELETE FROM " + mTableName + " WHERE transaction_uuid=$1;";
    const char *params[1]; int lengths[1]; int formats[1]={1};
    params[0]=reinterpret_cast<const char*>(transactionUUID.data); lengths[0]=TransactionUUID::kBytesSize;
    PGresult *res=PQexecParams(mDataBase,query.c_str(),1,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"deleteRecordIfExists");
    PQclear(res);
}

vector<BytesShared> TransactionsHandlerPostgreSQL::allTransactions()
{
    vector<BytesShared> result;
    const string query="SELECT transaction_body, transaction_bytes_count FROM " + mTableName + ";";
    PGresult *res = PQexecParams(mDataBase, query.c_str(), 0, nullptr, nullptr, nullptr, nullptr, 1);
    checkTuples(mDataBase,res,"allTransactions");
    int rows=PQntuples(res); result.reserve(rows);
    for (int i=0;i<rows;++i) {
        // Get bytes count from the integer field (4 bytes in network byte order)
        int bytesCountFieldLength = PQgetlength(res, i, 1);
        if (bytesCountFieldLength != 4) continue;
        
        uint32_t bytesCnt32 = ntohl(*reinterpret_cast<const uint32_t*>(PQgetvalue(res, i, 1)));
        size_t bytesCnt = static_cast<size_t>(bytesCnt32);
        
        if (bytesCnt==0) continue;
        BytesShared data=tryMalloc(bytesCnt);
        memcpy(data.get(), PQgetvalue(res,i,0), bytesCnt);
        result.push_back(data);
    }
    PQclear(res);
    return result;
}

LoggerStream TransactionsHandlerPostgreSQL::info() const { return mLog.info(logHeader()); }
LoggerStream TransactionsHandlerPostgreSQL::warning() const { return mLog.warning(logHeader()); }
const string TransactionsHandlerPostgreSQL::logHeader() const { stringstream s; s << "[TransactionsHandlerPostgreSQL]"; return s.str(); } 