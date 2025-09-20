#include "HistoryStoragePostgreSQL.h"
#include <sstream>
#include <cstring>
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

HistoryStoragePostgreSQL::HistoryStoragePostgreSQL(
    PGconn *dbConnection,
    const string &mainTableName,
    const string &additionalTableName,
    Logger &logger):
    mDataBase(dbConnection),
    mMainTableName(mainTableName),
    mAdditionalTableName(additionalTableName),
    mLog(logger)
{
    if (!mDataBase)
        throw ValueError("HistoryStoragePostgreSQL: db null");
    if (mMainTableName.empty() || mAdditionalTableName.empty())
        throw ValueError("HistoryStoragePostgreSQL: table names empty");

    // Create main table
    string query = "CREATE TABLE IF NOT EXISTS " + mMainTableName +
                   " (operation_uuid BYTEA NOT NULL, "
                   "operation_timestamp BIGINT NOT NULL, "
                   "record_type INT NOT NULL, "
                   "record_body BYTEA NOT NULL, "
                   "record_body_bytes_count INT NOT NULL, "
                   "equivalent INT NOT NULL, "
                   "command_uuid BYTEA);";
    PGresult *res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase,res,"HistoryStoragePG::create main table");
    PQclear(res);

    // Indexes for main table
    const vector<string> mainIndexes = {
        "CREATE INDEX IF NOT EXISTS " + mMainTableName + "_operation_uuid_idx ON " + mMainTableName + "(operation_uuid);",
        "CREATE INDEX IF NOT EXISTS " + mMainTableName + "_operation_timestamp_idx ON " + mMainTableName + "(operation_timestamp);",
        "CREATE INDEX IF NOT EXISTS " + mMainTableName + "_record_type_idx ON " + mMainTableName + "(record_type);",
        "CREATE INDEX IF NOT EXISTS " + mMainTableName + "_command_uuid_idx ON " + mMainTableName + "(command_uuid);",
        "CREATE INDEX IF NOT EXISTS " + mMainTableName + "_equivalent_idx ON " + mMainTableName + "(equivalent);"
    };
    for (const auto &q: mainIndexes) {
        res = PQexec(mDataBase, q.c_str());
        checkCmd(mDataBase,res,"HistoryStoragePG::index main");
        PQclear(res);
    }

    // Create additional table
    query = "CREATE TABLE IF NOT EXISTS " + mAdditionalTableName +
            " (operation_uuid BYTEA NOT NULL, "
            "operation_timestamp BIGINT NOT NULL, "
            "record_type INT NOT NULL, "
            "record_body BYTEA NOT NULL, "
            "record_body_bytes_count INT NOT NULL, "
            "equivalent INT NOT NULL);";
    res = PQexec(mDataBase, query.c_str());
    checkCmd(mDataBase,res,"HistoryStoragePG::create additional table");
    PQclear(res);

    // Indexes for additional table
    const vector<string> addIndexes = {
        "CREATE INDEX IF NOT EXISTS " + mAdditionalTableName + "_operation_uuid_idx ON " + mAdditionalTableName + "(operation_uuid);",
        "CREATE INDEX IF NOT EXISTS " + mAdditionalTableName + "_operation_timestamp_idx ON " + mAdditionalTableName + "(operation_timestamp);",
        "CREATE INDEX IF NOT EXISTS " + mAdditionalTableName + "_record_type_idx ON " + mAdditionalTableName + "(record_type);",
        "CREATE INDEX IF NOT EXISTS " + mAdditionalTableName + "_equivalent_idx ON " + mAdditionalTableName + "(equivalent);"
    };
    for (const auto &q: addIndexes) {
        res = PQexec(mDataBase, q.c_str());
        checkCmd(mDataBase,res,"HistoryStoragePG::index additional");
        PQclear(res);
    }
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "HistoryStoragePostgreSQL initialized: mainTable=" << mMainTableName << ", additionalTable=" << mAdditionalTableName;
#endif
}

void HistoryStoragePostgreSQL::saveTrustLineRecord(
    TrustLineRecord::Shared record,
    const SerializedEquivalent equivalent)
{
    if (!record)
        throw ValueError("saveTrustLineRecord: record null");

    auto bodyPair = record->serializedHistoryRecordBody();
    GEOEpochTimestamp ts = microsecondsSinceGEOEpoch(record->timestamp());

    const string query = "INSERT INTO " + mMainTableName +
                         " (operation_uuid, operation_timestamp, record_type, record_body, record_body_bytes_count, equivalent) "
                         "VALUES ($1,$2,$3,$4,$5,$6);";

    const int kParams = 6;
    const char *params[kParams];
    int lengths[kParams];
    int formats[kParams];
    // operation_uuid (binary)
    BytesSerializer serializer;
    serializer.copy(record->operationUUID());
    auto serializedUUID = serializer.collect();
    params[0] = reinterpret_cast<const char*>(serializedUUID.first.get());
    lengths[0] = TransactionUUID::kBytesSize;
    formats[0] = 1;
    // timestamp
    string tsStr = to_string(ts); params[1]=tsStr.c_str(); lengths[1]=0; formats[1]=0;
    // record_type
    string rtStr = to_string(record->recordType()); params[2]=rtStr.c_str(); lengths[2]=0; formats[2]=0;
    // record_body (binary)
    params[3]=reinterpret_cast<const char*>(bodyPair.first.get()); lengths[3]=static_cast<int>(bodyPair.second); formats[3]=1;
    // record_body_bytes_count
    string szStr = to_string(bodyPair.second); params[4]=szStr.c_str(); lengths[4]=0; formats[4]=0;
    // equivalent
    string eqStr = to_string(equivalent); params[5]=eqStr.c_str(); lengths[5]=0; formats[5]=0;

    PGresult *res = PQexecParams(mDataBase, query.c_str(), kParams, nullptr, params, lengths, formats, 0);
    checkCmd(mDataBase,res,"saveTrustLineRecord");
    PQclear(res);
}

void HistoryStoragePostgreSQL::savePaymentMainOutgoingRecord(
    PaymentRecord::Shared record,
    const SerializedEquivalent equivalent)
{
    auto bodyPair = record->serializedHistoryRecordBody();
    GEOEpochTimestamp ts = microsecondsSinceGEOEpoch(record->timestamp());

    const string query = "INSERT INTO " + mMainTableName +
                         " (operation_uuid, operation_timestamp, equivalent, record_type, record_body, record_body_bytes_count, command_uuid) "
                         "VALUES ($1,$2,$3,$4,$5,$6,$7);";

    const int kParams = 7;
    const char *params[kParams]; int lengths[kParams]; int formats[kParams];
    BytesSerializer serializer1;
    serializer1.copy(record->operationUUID());
    auto serializedUUID1 = serializer1.collect();
    params[0]=reinterpret_cast<const char*>(serializedUUID1.first.get()); lengths[0]=TransactionUUID::kBytesSize; formats[0]=1;
    string tsStr=to_string(ts); params[1]=tsStr.c_str(); lengths[1]=0; formats[1]=0;
    string eqStr=to_string(equivalent); params[2]=eqStr.c_str(); lengths[2]=0; formats[2]=0;
    string rtStr=to_string(record->recordType()); params[3]=rtStr.c_str(); lengths[3]=0; formats[3]=0;
    params[4]=reinterpret_cast<const char*>(bodyPair.first.get()); lengths[4]=static_cast<int>(bodyPair.second); formats[4]=1;
    string szStr=to_string(bodyPair.second); params[5]=szStr.c_str(); lengths[5]=0; formats[5]=0;
    BytesSerializer serializer2;
    serializer2.copy(record->commandUUID());
    auto serializedCommandUUID = serializer2.collect();
    params[6]=reinterpret_cast<const char*>(serializedCommandUUID.first.get()); lengths[6]=CommandUUID::kBytesSize; formats[6]=1;

    PGresult *res=PQexecParams(mDataBase,query.c_str(),kParams,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"savePaymentMainOutgoingRecord");
    PQclear(res);
}

void HistoryStoragePostgreSQL::savePaymentMainIncomingRecord(
    PaymentRecord::Shared record,
    const SerializedEquivalent equivalent)
{
    auto bodyPair = record->serializedHistoryRecordBody();
    GEOEpochTimestamp ts = microsecondsSinceGEOEpoch(record->timestamp());

    const string query = "INSERT INTO " + mMainTableName +
                         " (operation_uuid, operation_timestamp, equivalent, record_type, record_body, record_body_bytes_count) "
                         "VALUES ($1,$2,$3,$4,$5,$6);";

    const int kParams = 6;
    const char *params[kParams]; int lengths[kParams]; int formats[kParams];
    BytesSerializer serializer3;
    serializer3.copy(record->operationUUID());
    auto serializedUUID3 = serializer3.collect();
    params[0]=reinterpret_cast<const char*>(serializedUUID3.first.get()); lengths[0]=TransactionUUID::kBytesSize; formats[0]=1;
    string tsStr=to_string(ts); params[1]=tsStr.c_str(); lengths[1]=0; formats[1]=0;
    string eqStr=to_string(equivalent); params[2]=eqStr.c_str(); lengths[2]=0; formats[2]=0;
    string rtStr=to_string(record->recordType()); params[3]=rtStr.c_str(); lengths[3]=0; formats[3]=0;
    params[4]=reinterpret_cast<const char*>(bodyPair.first.get()); lengths[4]=static_cast<int>(bodyPair.second); formats[4]=1;
    string szStr=to_string(bodyPair.second); params[5]=szStr.c_str(); lengths[5]=0; formats[5]=0;

    PGresult *res=PQexecParams(mDataBase,query.c_str(),kParams,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"savePaymentMainIncomingRecord");
    PQclear(res);
}

void HistoryStoragePostgreSQL::savePaymentRecord(
    PaymentRecord::Shared record)
{
    if (!record) throw ValueError("savePaymentRecord: record null");
    switch (record->paymentOperationType()) {
        case PaymentRecord::OutgoingPaymentType:
            savePaymentMainOutgoingRecord(record, record->equivalent());
            break;
        case PaymentRecord::IncomingPaymentType:
            savePaymentMainIncomingRecord(record, record->equivalent());
            break;
        default:
            throw ValueError("savePaymentRecord: invalid operation type");
    }
}

void HistoryStoragePostgreSQL::savePaymentAdditionalRecord(
    PaymentAdditionalRecord::Shared record,
    const SerializedEquivalent equivalent)
{
    if (!record) throw ValueError("savePaymentAdditionalRecord: record null");
    auto bodyPair = record->serializedHistoryRecordBody();
    GEOEpochTimestamp ts = microsecondsSinceGEOEpoch(record->timestamp());

    const string query = "INSERT INTO " + mAdditionalTableName +
                         " (operation_uuid, operation_timestamp, equivalent, record_type, record_body, record_body_bytes_count) "
                         "VALUES ($1,$2,$3,$4,$5,$6);";
    const int kParams=6;
    const char *params[kParams]; int lengths[kParams]; int formats[kParams];
    BytesSerializer serializer4;
    serializer4.copy(record->operationUUID());
    auto serializedUUID4 = serializer4.collect();
    params[0]=reinterpret_cast<const char*>(serializedUUID4.first.get()); lengths[0]=TransactionUUID::kBytesSize; formats[0]=1;
    string tsStr=to_string(ts); params[1]=tsStr.c_str(); lengths[1]=0; formats[1]=0;
    string eqStr=to_string(equivalent); params[2]=eqStr.c_str(); lengths[2]=0; formats[2]=0;
    string rtStr=to_string(record->recordType()); params[3]=rtStr.c_str(); lengths[3]=0; formats[3]=0;
    params[4]=reinterpret_cast<const char*>(bodyPair.first.get()); lengths[4]=static_cast<int>(bodyPair.second); formats[4]=1;
    string szStr=to_string(bodyPair.second); params[5]=szStr.c_str(); lengths[5]=0; formats[5]=0;
    PGresult *res=PQexecParams(mDataBase,query.c_str(),kParams,nullptr,params,lengths,formats,0);
    checkCmd(mDataBase,res,"savePaymentAdditionalRecord");
    PQclear(res);
}

// ---------------- Retrieval helpers (minimal implementation for compilation) ----------------

std::vector<TrustLineRecord::Shared> HistoryStoragePostgreSQL::allTrustLineRecords(
    const SerializedEquivalent /*equivalent*/,
    size_t /*recordsCount*/,
    size_t /*fromRecord*/,
    DateTime /*timeFrom*/,
    bool /*isTimeFromPresent*/,
    DateTime /*timeTo*/,
    bool /*isTimeToPresent*/)
{
    return {};
}

std::vector<PaymentRecord::Shared> HistoryStoragePostgreSQL::allPaymentRecords(
    const SerializedEquivalent /*equivalent*/,
    size_t /*recordsCount*/,
    size_t /*fromRecord*/,
    DateTime /*timeFrom*/,
    bool /*isTimeFromPresent*/,
    DateTime /*timeTo*/,
    bool /*isTimeToPresent*/,
    const TrustLineAmount& /*lowBoundaryAmount*/,
    bool /*isLowBoundaryAmountPresent*/,
    const TrustLineAmount& /*highBoundaryAmount*/,
    bool /*isHighBoundaryAmountPresent*/)
{
    return {};
}

std::vector<PaymentRecord::Shared> HistoryStoragePostgreSQL::paymentRecordsAllEquivalents(
    size_t /*recordsCount*/,
    size_t /*fromRecord*/,
    DateTime /*timeFrom*/,
    bool /*isTimeFromPresent*/,
    DateTime /*timeTo*/,
    bool /*isTimeToPresent*/,
    const TrustLineAmount& /*lowBoundaryAmount*/,
    bool /*isLowBoundaryAmountPresent*/,
    const TrustLineAmount& /*highBoundaryAmount*/,
    bool /*isHighBoundaryAmountPresent*/)
{
    return {};
}

std::vector<PaymentRecord::Shared> HistoryStoragePostgreSQL::paymentRecordsByCommandUUID(
    const CommandUUID &/*commandUUID*/)
{
    return {};
}

std::vector<PaymentRecord::Shared> HistoryStoragePostgreSQL::paymentRecordsByTransactionUUID(
    const TransactionUUID &/*transactionUUID*/)
{
    return {};
}

std::vector<PaymentAdditionalRecord::Shared> HistoryStoragePostgreSQL::allPaymentAdditionalRecords(
    const SerializedEquivalent /*equivalent*/,
    size_t /*recordsCount*/,
    size_t /*fromRecord*/,
    DateTime /*timeFrom*/,
    bool /*isTimeFromPresent*/,
    DateTime /*timeTo*/,
    bool /*isTimeToPresent*/,
    const TrustLineAmount& /*lowBoundaryAmount*/,
    bool /*isLowBoundaryAmountPresent*/,
    const TrustLineAmount& /*highBoundaryAmount*/,
    bool /*isHighBoundaryAmountPresent*/)
{
    return {};
}

std::vector<Record::Shared> HistoryStoragePostgreSQL::recordsWithContractor(
    std::vector<BaseAddress::Shared> /*contractorAddresses*/,
    const SerializedEquivalent /*equivalent*/,
    size_t /*recordsCount*/,
    size_t /*fromRecord*/)
{
    return {};
}

bool HistoryStoragePostgreSQL::whetherOperationWasConducted(
    const TransactionUUID &transactionUUID)
{
    const string query = "SELECT 1 FROM " + mMainTableName + " WHERE operation_uuid=$1 LIMIT 1;";
    const char *params[1]; int lengths[1]; int formats[1]={1};
    BytesSerializer serializer;
    serializer.copy(transactionUUID);
    auto serializedUUID = serializer.collect();
    params[0]=reinterpret_cast<const char*>(serializedUUID.first.get()); lengths[0]=TransactionUUID::kBytesSize;
    PGresult *res = PQexecParams(mDataBase, query.c_str(), 1, nullptr, params, lengths, formats, 0);
    checkTuples(mDataBase,res,"whetherOperationWasConducted");
    bool found = PQntuples(res) > 0;
    PQclear(res);
    return found;
}

// Counts (stubbed for compilation)
size_t HistoryStoragePostgreSQL::countRecordsByType(
    Record::RecordType /*recordType*/,
    const SerializedEquivalent /*equivalent*/)
{
    return 0;
}
size_t HistoryStoragePostgreSQL::countRecordsByTypeAllEquivalents(
    Record::RecordType /*recordType*/)
{
    return 0;
}

// Portion stub
std::vector<Record::Shared> HistoryStoragePostgreSQL::recordsPortionWithContractor(
    std::vector<BaseAddress::Shared> /*contractorAddresses*/,
    size_t /*recordsCount*/,
    size_t /*fromRecord*/)
{
    return {};
}

// Deserialization helpers (binary)
TrustLineRecord::Shared HistoryStoragePostgreSQL::deserializeTrustLineRecord(
    PGresult * /*res*/, int /*rowIdx*/)
{
    return nullptr;
}
PaymentRecord::Shared HistoryStoragePostgreSQL::deserializePaymentRecord(
    PGresult * /*res*/, int /*rowIdx*/)
{
    return nullptr;
}
PaymentAdditionalRecord::Shared HistoryStoragePostgreSQL::deserializePaymentAdditionalRecord(
    PGresult * /*res*/, int /*rowIdx*/)
{
    return nullptr;
}

LoggerStream HistoryStoragePostgreSQL::info() const { return mLog.info(logHeader()); }
LoggerStream HistoryStoragePostgreSQL::debug() const { return mLog.debug(logHeader()); }
LoggerStream HistoryStoragePostgreSQL::warning() const { return mLog.warning(logHeader()); }
const string HistoryStoragePostgreSQL::logHeader() const {
    stringstream s; s << "[HistoryStoragePostgreSQL]"; return s.str();
} 