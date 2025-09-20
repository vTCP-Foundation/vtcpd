#include "PaymentParticipantsVotesHandlerPostgreSQL.h"
#include <sstream>
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

PaymentParticipantsVotesHandlerPostgreSQL::PaymentParticipantsVotesHandlerPostgreSQL(
    PGconn *dbConnection,
    const string &tableName,
    Logger &logger) :
    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    if (!mDataBase) throw ValueError("PaymentParticipantsVotesHandlerPostgreSQL: db conn null");
    if (mTableName.empty()) throw ValueError("PaymentParticipantsVotesHandlerPostgreSQL: table name empty");

    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (transaction_uuid BYTEA NOT NULL, "
                   "contractor BYTEA NOT NULL, "
                   "payment_node_id INTEGER NOT NULL, "
                   "public_key BYTEA NOT NULL, "
                   "signature BYTEA NOT NULL);";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase,res,"PaymentsVotes::create table");
    PQclear(res);

    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_transaction_uuid_idx ON " + mTableName + "(transaction_uuid);";
    res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase,res,"PaymentsVotes::index uuid");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "PaymentParticipantsVotesHandlerPostgreSQL initialized table=" << mTableName;
#endif
}

void PaymentParticipantsVotesHandlerPostgreSQL::saveRecord(
    const TransactionUUID &transactionUUID,
    Contractor::Shared contractor,
    const PaymentNodeID paymentNodeID,
    const PublicKey::Shared publicKey,
    const Signature::Shared signature)
{
    if (!contractor || !publicKey || !signature) {
        throw ValueError("saveRecord: null param");
    }

    const string query = "INSERT INTO " + mTableName +
                         "(transaction_uuid, contractor, payment_node_id, public_key, signature) "
                         "VALUES ($1,$2,$3,$4,$5);";
    const int kParams=5;
    const char *params[kParams];
    int lengths[kParams];
    int formats[kParams]={1,1,0,1,1};

    BytesSerializer serializer;
    serializer.copy(transactionUUID);
    auto serializedUUID = serializer.collect();
    params[0]=reinterpret_cast<const char*>(serializedUUID.first.get()); lengths[0]=TransactionUUID::kBytesSize;
    auto contractorBytes = contractor->serializeToBytes();
    params[1]=reinterpret_cast<const char*>(contractorBytes.get()); lengths[1]=contractor->serializedSize();
    string nodeStr=to_string(paymentNodeID); params[2]=nodeStr.c_str(); lengths[2]=0;
    params[3]=reinterpret_cast<const char*>(publicKey->data()); lengths[3]=publicKey->keySize();
    params[4]=reinterpret_cast<const char*>(signature->data()); lengths[4]=signature->signatureSize();

    PGresult *res = PQexecParams(mDataBase, query.c_str(),kParams,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"saveRecord");
    PQclear(res);
}

map<PaymentNodeID, Signature::Shared> PaymentParticipantsVotesHandlerPostgreSQL::participantsSignatures(
    const TransactionUUID &transactionUUID)
{
    const string query="SELECT payment_node_id, signature FROM " + mTableName + " WHERE transaction_uuid=$1;";
    const char *params[1]; int lengths[1]; int formats[1]={1};
    BytesSerializer serializer;
    serializer.copy(transactionUUID);
    auto serializedUUID = serializer.collect();
    params[0]=reinterpret_cast<const char*>(serializedUUID.first.get()); lengths[0]=TransactionUUID::kBytesSize;
    PGresult *res = PQexecParams(mDataBase, query.c_str(),1,nullptr,params,lengths,formats,0);
    checkTuples(mDataBase,res,"participantsSignatures");
    map<PaymentNodeID, Signature::Shared> result;
    int rows=PQntuples(res);
    for (int i=0;i<rows;++i) {
        PaymentNodeID node = static_cast<PaymentNodeID>(atoi(PQgetvalue(res,i,0)));
        const unsigned char *sigBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res,i,1));
        result[node]=make_shared<Signature>(sigBytes);
    }
    PQclear(res);
    return result;
}

void PaymentParticipantsVotesHandlerPostgreSQL::deleteRecords(
    const TransactionUUID &transactionUUID)
{
    const string query="DELETE FROM " + mTableName + " WHERE transaction_uuid=$1;";
    const char *params[1]; int lengths[1]; int formats[1]={1};
    BytesSerializer serializer;
    serializer.copy(transactionUUID);
    auto serializedUUID = serializer.collect();
    params[0]=reinterpret_cast<const char*>(serializedUUID.first.get()); lengths[0]=TransactionUUID::kBytesSize;
    PGresult *res = PQexecParams(mDataBase, query.c_str(),1,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"deleteRecords");
    PQclear(res);
}

LoggerStream PaymentParticipantsVotesHandlerPostgreSQL::info() const { return mLog.info(logHeader()); }
LoggerStream PaymentParticipantsVotesHandlerPostgreSQL::warning() const { return mLog.warning(logHeader()); }
const string PaymentParticipantsVotesHandlerPostgreSQL::logHeader() const { stringstream s; s << "[PaymentParticipantsVotesHandlerPostgreSQL]"; return s.str(); } 