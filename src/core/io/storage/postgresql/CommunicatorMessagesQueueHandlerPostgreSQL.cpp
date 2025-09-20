#include "CommunicatorMessagesQueueHandlerPostgreSQL.h"
#include <sstream>
#include "../../../common/time/TimeUtils.h"
#include "../../../common/serialization/BytesSerializer.h"

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

CommunicatorMessagesQueueHandlerPostgreSQL::CommunicatorMessagesQueueHandlerPostgreSQL(
    PGconn *dbConnection,
    const string &tableName,
    Logger &logger):
    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    if (!mDataBase) throw ValueError("CommunicatorMessagesQueueHandlerPostgreSQL: db null");
    if (mTableName.empty()) throw ValueError("CommunicatorMessagesQueueHandlerPostgreSQL: tableName empty");

    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (contractor_id INT NOT NULL, "
                   "equivalent INT NOT NULL, "
                   "transaction_uuid BYTEA NOT NULL, "
                   "message_type INT NOT NULL, "
                   "message BYTEA NOT NULL, "
                   "message_bytes_count INT NOT NULL, "
                   "recording_time BIGINT NOT NULL, "
                   "FOREIGN KEY(contractor_id) REFERENCES contractors(id) ON DELETE CASCADE ON UPDATE CASCADE);";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase, res, "CMQHPG::create table");
    PQclear(res);
}

void CommunicatorMessagesQueueHandlerPostgreSQL::saveRecord(
    ContractorID contractorID,
    const SerializedEquivalent equivalent,
    const TransactionUUID &transactionUUID,
    const Message::SerializedType messageType,
    BytesShared message,
    size_t messageBytesCount)
{
    if (!message) throw ValueError("saveRecord: message null");
    const string query = "INSERT INTO " + mTableName +
                         " (contractor_id, equivalent, transaction_uuid, message_type, message, message_bytes_count, recording_time) "
                         "VALUES ($1,$2,$3,$4,$5,$6,$7);";
    const int kParams=7;
    const char *params[kParams]; int lengths[kParams]; int formats[kParams];
    string contractorStr=to_string(contractorID); params[0]=contractorStr.c_str(); lengths[0]=0; formats[0]=0;
    string eqStr=to_string(equivalent); params[1]=eqStr.c_str(); lengths[1]=0; formats[1]=0;
    BytesSerializer serializer;
    serializer.copy(transactionUUID);
    auto serializedUUID = serializer.collect();
    params[2]=reinterpret_cast<const char*>(serializedUUID.first.get()); lengths[2]=TransactionUUID::kBytesSize; formats[2]=1;
    string typeStr=to_string((int)messageType); params[3]=typeStr.c_str(); lengths[3]=0; formats[3]=0;
    params[4]=reinterpret_cast<const char*>(message.get()); lengths[4]=static_cast<int>(messageBytesCount); formats[4]=1;
    string bytesStr=to_string(messageBytesCount); params[5]=bytesStr.c_str(); lengths[5]=0; formats[5]=0;
    GEOEpochTimestamp ts = microsecondsSinceGEOEpoch(utc_now());
    string tsStr=to_string(ts); params[6]=tsStr.c_str(); lengths[6]=0; formats[6]=0;

    PGresult *res = PQexecParams(mDataBase, query.c_str(), kParams, nullptr, params, lengths, formats, 0);
    if (PQresultStatus(res)==PGRES_COMMAND_OK) {
        // ok
    } else if (PQresultStatus(res)==PGRES_FATAL_ERROR || PQresultStatus(res)==PGRES_BAD_RESPONSE) {
        string err=PQresultErrorMessage(res);
        PQclear(res);
        throw IOError("saveRecord: " + err);
    }
    PQclear(res);
}

vector<tuple<ContractorID, BytesShared, Message::SerializedType>> CommunicatorMessagesQueueHandlerPostgreSQL::allMessages()
{
    vector<tuple<ContractorID, BytesShared, Message::SerializedType>> result;
    string query = "SELECT contractor_id, message_type, message, message_bytes_count FROM " + mTableName + ";";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkTuples(mDataBase,res,"allMessages");
    int rows = PQntuples(res);
    result.reserve(rows);
    for (int i=0;i<rows;++i){
        ContractorID contractorID = static_cast<ContractorID>(atoi(PQgetvalue(res,i,0)));
        Message::SerializedType msgType = static_cast<Message::SerializedType>(atoi(PQgetvalue(res,i,1)));
        size_t bytesCnt = static_cast<size_t>(atoi(PQgetvalue(res,i,3)));
        BytesShared msg = tryMalloc(bytesCnt);
        memcpy(msg.get(), PQgetvalue(res,i,2), bytesCnt);
        result.emplace_back(contractorID, msg, msgType);
    }
    PQclear(res);
    return result;
}

void CommunicatorMessagesQueueHandlerPostgreSQL::deleteRecord(
    ContractorID contractorID,
    const SerializedEquivalent equivalent,
    const Message::SerializedType messageType)
{
    string query = "DELETE FROM " + mTableName + " WHERE contractor_id=$1 AND equivalent=$2 AND message_type=$3;";
    const char *params[3]; int lengths[3]={0,0,0}; int formats[3]={0,0,0};
    string cStr=to_string(contractorID); params[0]=cStr.c_str();
    string eStr=to_string(equivalent); params[1]=eStr.c_str();
    string tStr=to_string((int)messageType); params[2]=tStr.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),3,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"deleteRecord equiv");
    PQclear(res);
}

void CommunicatorMessagesQueueHandlerPostgreSQL::deleteRecord(
    ContractorID contractorID,
    const TransactionUUID &transactionUUID)
{
    string query = "DELETE FROM " + mTableName + " WHERE contractor_id=$1 AND transaction_uuid=$2;";
    const char *params[2]; int lengths[2]; int formats[2]={0,1};
    string cStr=to_string(contractorID); params[0]=cStr.c_str(); lengths[0]=0;
    BytesSerializer serializer2;
    serializer2.copy(transactionUUID);
    auto serializedUUID2 = serializer2.collect();
    params[1]=reinterpret_cast<const char*>(serializedUUID2.first.get()); lengths[1]=TransactionUUID::kBytesSize;
    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"deleteRecord uuid");
    PQclear(res);
}

LoggerStream CommunicatorMessagesQueueHandlerPostgreSQL::info() const { return mLog.info(logHeader()); }
LoggerStream CommunicatorMessagesQueueHandlerPostgreSQL::warning() const { return mLog.warning(logHeader()); }
const string CommunicatorMessagesQueueHandlerPostgreSQL::logHeader() const { stringstream s; s << "[CommunicatorMessagesQueueHandlerPG]"; return s.str(); } 