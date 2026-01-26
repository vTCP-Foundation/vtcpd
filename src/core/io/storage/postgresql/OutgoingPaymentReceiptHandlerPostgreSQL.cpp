#include "OutgoingPaymentReceiptHandlerPostgreSQL.h"
#include "../interfaces/PaymentTransactionsHandler.h"
#include "StorageHandlerPostgreSQL.h"
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

OutgoingPaymentReceiptHandlerPostgreSQL::OutgoingPaymentReceiptHandlerPostgreSQL(
    PGconn *dbConnection,
    const string &tableName,
    Logger &logger) :
    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    if (!mDataBase) {
        throw ValueError("OutgoingPaymentReceiptHandlerPostgreSQL: db connection null");
    }
    if (mTableName.empty()) {
        throw ValueError("OutgoingPaymentReceiptHandlerPostgreSQL: table name empty");
    }

    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (trust_line_id INTEGER NOT NULL, "
                   "audit_number INTEGER NOT NULL, "
                   "transaction_uuid BYTEA NOT NULL, "
                   "own_public_key_hash BYTEA NOT NULL, "
                   "amount BYTEA NOT NULL, "
                   "FOREIGN KEY(trust_line_id) REFERENCES trust_lines(id) ON DELETE CASCADE ON UPDATE CASCADE, "
                   "FOREIGN KEY(own_public_key_hash) REFERENCES own_keys(hash) ON DELETE CASCADE ON UPDATE CASCADE);";

    PGresult *res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase, res, "OutgoingPaymentReceiptHandlerPostgreSQL::create table");
    PQclear(res);

    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_transaction_uuid_idx ON " + mTableName + "(transaction_uuid);";
    res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase, res, "OutgoingPaymentReceiptHandlerPostgreSQL::uuid idx");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "OutgoingPaymentReceiptHandlerPostgreSQL initialized: table=" << mTableName;
#endif
}

void OutgoingPaymentReceiptHandlerPostgreSQL::saveRecord(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber,
    const TransactionUUID &transactionUUID,
    const KeyHash::Shared ownPublicKeyHash,
    const TrustLineAmount &amount)
{
    if (!ownPublicKeyHash) {
        throw ValueError("saveRecord: ownPublicKeyHash null");
    }

    const string query = "INSERT INTO " + mTableName +
                         "(trust_line_id, audit_number, transaction_uuid, own_public_key_hash, amount) "
                         "VALUES ($1,$2,$3,$4,$5);";

    const int kParams = 5;
    const char *params[kParams];
    int lengths[kParams];
    int formats[kParams] = {0,0,1,1,1};

    string tlIdStr = to_string(trustLineID);
    string auditStr = to_string(auditNumber);

    params[0] = tlIdStr.c_str(); lengths[0]=0;
    params[1] = auditStr.c_str(); lengths[1]=0;

    params[2] = reinterpret_cast<const char*>(transactionUUID.data); lengths[2]=TransactionUUID::kBytesSize;

    params[3] = reinterpret_cast<const char*>(ownPublicKeyHash->data()); lengths[3]=KeyHash::kBytesSize;

    vector<byte_t> amountBytes = trustLineAmountToBytes(amount);
    params[4] = reinterpret_cast<const char*>(amountBytes.data()); lengths[4]=kTrustLineAmountBytesCount;

    PGresult *res = PQexecParams(mDataBase, query.c_str(), kParams, nullptr, params, lengths, formats, 0);
    checkCmd(mDataBase, res, "Outgoing saveRecord");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Outgoing receipt saved TL=" << trustLineID << " audit=" << auditNumber;
#endif
}

map<TransactionUUID, TrustLineAmount> OutgoingPaymentReceiptHandlerPostgreSQL::auditAmounts(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber)
{
    map<TransactionUUID, TrustLineAmount> result;
    const string query = "SELECT transaction_uuid, amount FROM " + mTableName + " WHERE trust_line_id=$1 AND audit_number=$2;";

    const char *params[2]; int lengths[2]={0,0}; int formats[2]={0,0};
    string tlIdStr=to_string(trustLineID); string auditStr=to_string(auditNumber);
    params[0]=tlIdStr.c_str(); params[1]=auditStr.c_str();

    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,params,lengths,formats,1);
    checkTuples(mDataBase,res,"auditAmounts");

    int rows = PQntuples(res);
    for (int i=0;i<rows;++i) {
        const unsigned char *uuidBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res,i,0));
        const unsigned char *amountBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res,i,1));

        TransactionUUID tUUID(uuidBytes);
        vector<byte_t> amountBuf(amountBytes, amountBytes + kTrustLineAmountBytesCount);
        result[tUUID] = bytesToTrustLineAmount(amountBuf);
    }
    PQclear(res);
    return result;
}

vector<ReceiptRecord::Shared> OutgoingPaymentReceiptHandlerPostgreSQL::receiptsByAuditNumber(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber)
{
    vector<ReceiptRecord::Shared> result;
    const string query = "SELECT amount, transaction_uuid, own_public_key_hash FROM " + mTableName + " WHERE trust_line_id=$1 AND audit_number=$2;";

    const char *params[2]; int lengths[2]={0,0}; int formats[2]={0,0};
    string tlIdStr=to_string(trustLineID); string auditStr=to_string(auditNumber);
    params[0]=tlIdStr.c_str(); params[1]=auditStr.c_str();

    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,params,lengths,formats,1);
    checkTuples(mDataBase,res,"receiptsByAuditNumber");

    int rows = PQntuples(res);
    for (int i=0;i<rows;++i) {
        const unsigned char *amountBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res,i,0));
        const unsigned char *uuidBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res,i,1));
        const unsigned char *keyHashBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res,i,2));

        vector<byte_t> amountBuf(amountBytes, amountBytes + kTrustLineAmountBytesCount);
        TransactionUUID tUUID(uuidBytes);
        auto keyHash = make_shared<KeyHash>(keyHashBytes);

        result.push_back(make_shared<ReceiptRecord>(auditNumber, tUUID, bytesToTrustLineAmount(amountBuf), keyHash, nullptr));
    }
    PQclear(res);
    return result;
}

vector<ReceiptRecord::Shared> OutgoingPaymentReceiptHandlerPostgreSQL::receiptsLessEqualThanAuditNumber(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber)
{
    vector<ReceiptRecord::Shared> result;
    const string query = "SELECT amount, transaction_uuid, own_public_key_hash, audit_number FROM " + mTableName + " WHERE trust_line_id=$1 AND audit_number<=$2;";

    const char *params[2]; int lengths[2]={0,0}; int formats[2]={0,0};
    string tlIdStr=to_string(trustLineID); string auditStr=to_string(auditNumber);
    params[0]=tlIdStr.c_str(); params[1]=auditStr.c_str();

    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,params,lengths,formats,1);
    checkTuples(mDataBase,res,"receiptsLE");

    int rows = PQntuples(res);
    for (int i=0;i<rows;++i) {
        const unsigned char *amountBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res,i,0));
        const unsigned char *uuidBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res,i,1));
        const unsigned char *keyHashBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res,i,2));
        AuditNumber currentAudit = static_cast<AuditNumber>(atoi(PQgetvalue(res,i,3)));

        vector<byte_t> amountBuf(amountBytes, amountBytes + kTrustLineAmountBytesCount);
        TransactionUUID tUUID(uuidBytes);
        auto keyHash = make_shared<KeyHash>(keyHashBytes);

        result.push_back(make_shared<ReceiptRecord>(currentAudit, tUUID, bytesToTrustLineAmount(amountBuf), keyHash, nullptr));
    }
    PQclear(res);
    return result;
}

vector<ReceiptRecord::Shared> OutgoingPaymentReceiptHandlerPostgreSQL::getFinalizedReceiptsWithZeroAuditNumber(
    const TrustLineID trustLineID)
{
    vector<ReceiptRecord::Shared> result;

    // Join receipts with payment transactions to filter by finalized observing state.
    const string query = "SELECT r.amount, r.transaction_uuid, r.own_public_key_hash FROM " + mTableName +
        " r JOIN " + string(StorageHandlerPostgreSQL::paymentTransactionsTableName()) +
        " pt ON r.transaction_uuid = pt.uuid WHERE r.trust_line_id=$1 AND r.audit_number=$2 AND pt.observing_state=$3;";

    const int kParams = 3;
    const char *params[kParams];
    int lengths[kParams] = {0, 0, 0};
    int formats[kParams] = {0, 0, 0};

    string tlIdStr = to_string(trustLineID);
    string auditStr = to_string(kZeroAuditNumber);
    string observingStateStr = to_string(kCommittedObservingState);
    params[0] = tlIdStr.c_str();
    params[1] = auditStr.c_str();
    params[2] = observingStateStr.c_str();

    PGresult *res = PQexecParams(mDataBase, query.c_str(), kParams, nullptr, params, lengths, formats, 1);
    checkTuples(mDataBase, res, "getFinalizedReceiptsWithZeroAuditNumber");

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        const unsigned char *amountBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res, i, 0));
        const unsigned char *uuidBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res, i, 1));
        const unsigned char *keyHashBytes = reinterpret_cast<const unsigned char*>(PQgetvalue(res, i, 2));

        vector<byte_t> amountBuf(amountBytes, amountBytes + kTrustLineAmountBytesCount);
        TransactionUUID tUUID(uuidBytes);
        auto keyHash = make_shared<KeyHash>(keyHashBytes);

        result.push_back(make_shared<ReceiptRecord>(
            kZeroAuditNumber,
            tUUID,
            bytesToTrustLineAmount(amountBuf),
            keyHash,
            nullptr));
    }
    PQclear(res);

    return result;
}

void OutgoingPaymentReceiptHandlerPostgreSQL::updateAuditNumberByTransactionUUIDs(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber,
    const vector<TransactionUUID> &transactionUUIDs)
{
    // Avoid generating invalid SQL when there are no transaction UUIDs to update.
    if (transactionUUIDs.empty()) {
        return;
    }

    string query = "UPDATE " + mTableName + " SET audit_number=$1 WHERE trust_line_id=$2 AND transaction_uuid IN (";
    for (size_t i = 0; i < transactionUUIDs.size(); ++i) {
        query += "$" + to_string(static_cast<int>(i) + 3);
        if (i + 1 < transactionUUIDs.size()) {
            query += ", ";
        }
    }
    query += ");";

    const size_t paramsCount = 2 + transactionUUIDs.size();
    vector<const char*> params(paramsCount);
    vector<int> lengths(paramsCount);
    vector<int> formats(paramsCount);

    string auditStr = to_string(auditNumber);
    string tlIdStr = to_string(trustLineID);
    params[0] = auditStr.c_str();
    lengths[0] = 0;
    formats[0] = 0;
    params[1] = tlIdStr.c_str();
    lengths[1] = 0;
    formats[1] = 0;

    for (size_t i = 0; i < transactionUUIDs.size(); ++i) {
        params[i + 2] = reinterpret_cast<const char*>(transactionUUIDs[i].data);
        lengths[i + 2] = TransactionUUID::kBytesSize;
        formats[i + 2] = 1;
    }

    PGresult *res = PQexecParams(
        mDataBase,
        query.c_str(),
        static_cast<int>(paramsCount),
        nullptr,
        params.data(),
        lengths.data(),
        formats.data(),
        0);
    checkCmd(mDataBase, res, "updateAuditNumberByTransactionUUIDs");
    PQclear(res);
}

size_t OutgoingPaymentReceiptHandlerPostgreSQL::countReceiptsByNumber(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber)
{
    const string query = "SELECT COUNT(transaction_uuid) FROM " + mTableName + " WHERE trust_line_id=$1 AND audit_number=$2;";
    const char *params[2]; int lengths[2]={0,0}; int formats[2]={0,0};
    string tlIdStr=to_string(trustLineID); string auditStr=to_string(auditNumber);
    params[0]=tlIdStr.c_str(); params[1]=auditStr.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,params,lengths,formats,0);
    checkTuples(mDataBase,res,"countReceipts");
    if (PQntuples(res)==0) { PQclear(res); throw IOError("countReceiptsByNumber: no result"); }
    size_t count = static_cast<size_t>(atoi(PQgetvalue(res,0,0)));
    PQclear(res);
    return count;
}

void OutgoingPaymentReceiptHandlerPostgreSQL::deleteRecords(
    const TransactionUUID &transactionUUID)
{
    const string query = "DELETE FROM " + mTableName + " WHERE transaction_uuid=$1;";
    const char *params[1]; int lengths[1]; int formats[1]={1};
    params[0]=reinterpret_cast<const char*>(transactionUUID.data); lengths[0]=TransactionUUID::kBytesSize;
    PGresult *res = PQexecParams(mDataBase, query.c_str(),1,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"deleteRecords uuid");
    PQclear(res);
}

void OutgoingPaymentReceiptHandlerPostgreSQL::deleteRecords(
    const TrustLineID trustLineID)
{
    const string query = "DELETE FROM " + mTableName + " WHERE trust_line_id=$1;";
    const char *params[1]; int lengths[1]={0}; int formats[1]={0};
    string tlIdStr = to_string(trustLineID); params[0]=tlIdStr.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),1,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"deleteRecords TL");
    PQclear(res);
}

void OutgoingPaymentReceiptHandlerPostgreSQL::deleteRecordsByAuditNumber(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber)
{
    // Task 20-02: delete receipts scoped to trust line and audit number.
    const string query = "DELETE FROM " + mTableName + " WHERE trust_line_id=$1 AND audit_number=$2;";
    const char *params[2]; int lengths[2]={0,0}; int formats[2]={0,0};
    string tlIdStr = to_string(trustLineID);
    string auditStr = to_string(auditNumber);
    params[0]=tlIdStr.c_str();
    params[1]=auditStr.c_str();
    PGresult *res = PQexecParams(mDataBase, query.c_str(),2,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"deleteRecords audit number");
    PQclear(res);
}

bool OutgoingPaymentReceiptHandlerPostgreSQL::isContainsKeyHash(
    KeyHash::Shared keyHash)
{
    if (!keyHash) throw ValueError("isContainsKeyHash: keyHash null");
    const string query = "SELECT own_public_key_hash FROM " + mTableName + " WHERE own_public_key_hash=$1 LIMIT 1;";
    const char *params[1]; int lengths[1]; int formats[1]={1};
    params[0]=reinterpret_cast<const char*>(keyHash->data()); lengths[0]=KeyHash::kBytesSize;
    PGresult *res = PQexecParams(mDataBase, query.c_str(),1,nullptr,params,lengths,formats,0);
    checkTuples(mDataBase,res,"containsKeyHash");
    bool result = PQntuples(res) > 0;
    PQclear(res);
    return result;
}

bool OutgoingPaymentReceiptHandlerPostgreSQL::isContainsTransaction(
    const TransactionUUID &transactionUUID)
{
    const string query = "SELECT transaction_uuid FROM " + mTableName + " WHERE transaction_uuid=$1 LIMIT 1;";
    const char *params[1]; int lengths[1]; int formats[1]={1};
    params[0]=reinterpret_cast<const char*>(transactionUUID.data); lengths[0]=TransactionUUID::kBytesSize;
    PGresult *res = PQexecParams(mDataBase, query.c_str(),1,nullptr,params,lengths,formats,0);
    checkTuples(mDataBase,res,"containsTransaction");
    bool result = PQntuples(res) > 0;
    PQclear(res);
    return result;
}

LoggerStream OutgoingPaymentReceiptHandlerPostgreSQL::info() const { return mLog.info(logHeader()); }
LoggerStream OutgoingPaymentReceiptHandlerPostgreSQL::warning() const { return mLog.warning(logHeader()); }
const string OutgoingPaymentReceiptHandlerPostgreSQL::logHeader() const { stringstream s; s << "[OutgoingPaymentReceiptHandlerPostgreSQL]"; return s.str(); } 
