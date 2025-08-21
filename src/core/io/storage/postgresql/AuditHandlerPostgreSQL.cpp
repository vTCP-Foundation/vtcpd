#include "AuditHandlerPostgreSQL.h"
#include "../../../core/io/storage/record/audit/AuditRecord.h"
#include <sstream>
#include <arpa/inet.h>

using namespace std;

namespace {

inline void checkResultCmd(PGconn *db, PGresult *res, const string &errPrefix) {
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        string err = PQerrorMessage(db);
        PQclear(res);
        throw IOError(errPrefix + ": " + err);
    }
}

inline void checkResultTuples(PGconn *db, PGresult *res, const string &errPrefix) {
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        string err = PQerrorMessage(db);
        PQclear(res);
        throw IOError(errPrefix + ": " + err);
    }
}

} // anonymous namespace

AuditHandlerPostgreSQL::AuditHandlerPostgreSQL(
    PGconn *dbConnection,
    const string &tableName,
    Logger &logger) :

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    if (mDataBase == nullptr) {
        throw IOError("AuditHandlerPostgreSQL: Database connection is null");
    }

    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   "(number INTEGER NOT NULL, "
                   "trust_line_id INTEGER NOT NULL, "
                   "our_signature BYTEA NOT NULL, "
                   "contractor_signature BYTEA, "
                   "balance BYTEA NOT NULL, "
                   "outgoing_amount BYTEA NOT NULL, "
                   "incoming_amount BYTEA NOT NULL, "
                   "FOREIGN KEY(trust_line_id) REFERENCES trust_lines(id) ON DELETE CASCADE ON UPDATE CASCADE);";

    PGresult *res = PQexec(mDataBase, query.c_str());
    checkResultCmd(mDataBase, res, "AuditHandlerPostgreSQL::creating table");
    PQclear(res);

    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName + "_number_trust_line_id_idx on " + mTableName + "(number, trust_line_id);";
    res = PQexec(mDataBase, query.c_str());
    checkResultCmd(mDataBase, res, "AuditHandlerPostgreSQL::creating index");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "AuditHandler initialized: table=" << mTableName;
#endif
}

void AuditHandlerPostgreSQL::saveFullAudit(
    AuditNumber number,
    TrustLineID trustLineID,
    Signature::Shared ownSignature,
    Signature::Shared contractorSignature,
    const TrustLineAmount &incomingAmount,
    const TrustLineAmount &outgoingAmount,
    const TrustLineBalance &balance)
{
    const string query = "INSERT INTO " + mTableName +
                         "(number, trust_line_id, our_signature, contractor_signature, "
                         "incoming_amount, outgoing_amount, balance) "
                         "VALUES ($1, $2, $3, $4, $5, $6, $7);";

    vector<byte_t> incomingAmountBytes = trustLineAmountToBytes(incomingAmount);
    vector<byte_t> outgoingAmountBytes = trustLineAmountToBytes(outgoingAmount);
    vector<byte_t> balanceBytes = trustLineBalanceToBytes(const_cast<TrustLineBalance &>(balance));

    const int kParams = 7;
    const char *paramValues[kParams];
    int paramLengths[kParams];
    int paramFormats[kParams] = {0};

    string numStr = to_string(number);
    string tlIdStr = to_string(trustLineID);

    paramValues[0] = numStr.c_str(); // text
    paramValues[1] = tlIdStr.c_str();
    paramValues[2] = reinterpret_cast<const char *>(ownSignature->data());
    paramValues[3] = reinterpret_cast<const char *>(contractorSignature->data());
    paramValues[4] = reinterpret_cast<const char *>(incomingAmountBytes.data());
    paramValues[5] = reinterpret_cast<const char *>(outgoingAmountBytes.data());
    paramValues[6] = reinterpret_cast<const char *>(balanceBytes.data());

    paramLengths[0] = 0;
    paramLengths[1] = 0;
    paramLengths[2] = ownSignature->signatureSize();
    paramLengths[3] = contractorSignature->signatureSize();
    paramLengths[4] = kTrustLineAmountBytesCount;
    paramLengths[5] = kTrustLineAmountBytesCount;
    paramLengths[6] = kTrustLineBalanceSerializeBytesCount;

    // binary formats for blob fields
    for (int i = 2; i < kParams; ++i) {
        paramFormats[i] = 1;
    }

    PGresult *res = PQexecParams(
        mDataBase,
        query.c_str(),
        kParams,
        nullptr,
        paramValues,
        paramLengths,
        paramFormats,
        0);

    checkResultCmd(mDataBase, res, "AuditHandlerPostgreSQL::saveFullAudit");
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "prepare inserting is completed successfully";
#endif
}

void AuditHandlerPostgreSQL::saveOwnAuditPart(
    AuditNumber number,
    TrustLineID trustLineID,
    Signature::Shared ownSignature,
    const TrustLineAmount &incomingAmount,
    const TrustLineAmount &outgoingAmount,
    const TrustLineBalance &balance)
{
    const string query = "INSERT INTO " + mTableName +
                         "(number, trust_line_id, our_signature, incoming_amount, outgoing_amount, balance) VALUES ($1,$2,$3,$4,$5,$6);";

    vector<byte_t> incomingAmountBytes = trustLineAmountToBytes(incomingAmount);
    vector<byte_t> outgoingAmountBytes = trustLineAmountToBytes(outgoingAmount);
    vector<byte_t> balanceBytes = trustLineBalanceToBytes(const_cast<TrustLineBalance &>(balance));

    const int kParams = 6;
    const char *paramValues[kParams];
    int paramLengths[kParams];
    int paramFormats[kParams] = {0};

    string numStr = to_string(number);
    string tlIdStr = to_string(trustLineID);

    paramValues[0] = numStr.c_str();
    paramValues[1] = tlIdStr.c_str();
    paramValues[2] = reinterpret_cast<const char *>(ownSignature->data());
    paramValues[3] = reinterpret_cast<const char *>(incomingAmountBytes.data());
    paramValues[4] = reinterpret_cast<const char *>(outgoingAmountBytes.data());
    paramValues[5] = reinterpret_cast<const char *>(balanceBytes.data());

    paramLengths[0] = 0;
    paramLengths[1] = 0;
    paramLengths[2] = ownSignature->signatureSize();
    paramLengths[3] = kTrustLineAmountBytesCount;
    paramLengths[4] = kTrustLineAmountBytesCount;
    paramLengths[5] = kTrustLineBalanceSerializeBytesCount;

    for (int i = 2; i < kParams; ++i) {
        paramFormats[i] = 1;
    }

    PGresult *res = PQexecParams(mDataBase, query.c_str(), kParams, nullptr, paramValues, paramLengths, paramFormats, 0);
    checkResultCmd(mDataBase, res, "AuditHandlerPostgreSQL::saveOwnAuditPart");
    PQclear(res);
}

void AuditHandlerPostgreSQL::saveContractorAuditPart(
    AuditNumber number,
    TrustLineID trustLineID,
    Signature::Shared contractorSignature)
{
    const string query = "UPDATE " + mTableName +
                         " SET contractor_signature = $1 "
                         "WHERE trust_line_id = $2 AND number = $3;";

    const int kParams = 3;
    const char *paramValues[kParams];
    int paramLengths[kParams];
    int paramFormats[kParams] = {1,0,0};

    string tlIdStr = to_string(trustLineID);
    string numStr = to_string(number);

    paramValues[0] = reinterpret_cast<const char *>(contractorSignature->data());
    paramValues[1] = tlIdStr.c_str();
    paramValues[2] = numStr.c_str();

    paramLengths[0] = contractorSignature->signatureSize();
    paramLengths[1] = 0;
    paramLengths[2] = 0;

    PGresult *res = PQexecParams(mDataBase, query.c_str(), kParams, nullptr, paramValues, paramLengths, paramFormats, 0);
    checkResultCmd(mDataBase, res, "AuditHandlerPostgreSQL::saveContractorAuditPart");
    if (PQcmdTuples(res)[0] == '0') {
        PQclear(res);
        throw ValueError("No data were modified");
    }
    PQclear(res);
}

const AuditRecord::Shared AuditHandlerPostgreSQL::getActualAudit(
    TrustLineID trustLineID)
{
    const string query = "SELECT number, incoming_amount, outgoing_amount, balance, contractor_signature FROM " + mTableName +
                         " WHERE trust_line_id = $1 ORDER BY number DESC LIMIT 1;";

    const char *paramValues[1];
    int paramLengths[1] = {0};
    int paramFormats[1] = {0};
    string tlIdStr = to_string(trustLineID);
    paramValues[0] = tlIdStr.c_str();

    PGresult *res = PQexecParams(mDataBase, query.c_str(), 1, nullptr, paramValues, paramLengths, paramFormats, 1);
    checkResultTuples(mDataBase, res, "AuditHandlerPostgreSQL::getActualAudit");

    if (PQntuples(res) == 0) {
        PQclear(res);
        throw NotFoundError("No records with requested trust line id");
    }

    int idx = 0;
    // Convert binary format to integer correctly
    AuditNumber number;
    const unsigned char *numberBytes = reinterpret_cast<const unsigned char *>(PQgetvalue(res, 0, idx++));
    memcpy(&number, numberBytes, sizeof(AuditNumber));
    number = ntohl(number); // Convert from network byte order

    auto extractAmount = [&](int col){
        const unsigned char *bytes = reinterpret_cast<const unsigned char *>(PQgetvalue(res, 0, col));
        vector<byte_t> v(bytes, bytes + kTrustLineAmountBytesCount);
        return bytesToTrustLineAmount(v);
    };

    TrustLineAmount incomingAmount = extractAmount(idx++);
    TrustLineAmount outgoingAmount = extractAmount(idx++);

    const unsigned char *balanceBytesPtr = reinterpret_cast<const unsigned char *>(PQgetvalue(res, 0, idx++));
    vector<byte_t> balanceBuf(balanceBytesPtr, balanceBytesPtr + kTrustLineBalanceSerializeBytesCount);
    TrustLineBalance balance = bytesToTrustLineBalance(balanceBuf);

    const unsigned char *contractorSigPtr = reinterpret_cast<const unsigned char *>(PQgetvalue(res, 0, idx++));
    Signature::Shared contractorSignature = nullptr;
    if (contractorSigPtr) {
        contractorSignature = make_shared<Signature>(contractorSigPtr);
    }

    auto record = make_shared<AuditRecord>(number, incomingAmount, outgoingAmount, balance);
    record->setContractorSignature(contractorSignature);
    PQclear(res);
    return record;
}

const AuditRecord::Shared AuditHandlerPostgreSQL::getActualAuditFull(
    TrustLineID trustLineID)
{
    const string query = "SELECT number, incoming_amount, outgoing_amount, balance, "
                         "our_signature, contractor_signature FROM " + mTableName +
                         " WHERE trust_line_id = $1 ORDER BY number DESC LIMIT 1;";

    const char *paramValues[1];
    int paramLengths[1] = {0};
    int paramFormats[1] = {0};
    string tlIdStr = to_string(trustLineID);
    paramValues[0] = tlIdStr.c_str();

    PGresult *res = PQexecParams(mDataBase, query.c_str(), 1, nullptr, paramValues, paramLengths, paramFormats, 1);
    checkResultTuples(mDataBase, res, "AuditHandlerPostgreSQL::getActualAuditFull");

    if (PQntuples(res) == 0) {
        PQclear(res);
        throw NotFoundError("No records with requested trust line id");
    }

    int idx = 0;
    // Convert binary format to integer correctly
    AuditNumber number;
    const unsigned char *numberBytes = reinterpret_cast<const unsigned char *>(PQgetvalue(res, 0, idx++));
    memcpy(&number, numberBytes, sizeof(AuditNumber));
    number = ntohl(number); // Convert from network byte order

    auto extractAmount = [&](int col){
        const unsigned char *bytes = reinterpret_cast<const unsigned char *>(PQgetvalue(res, 0, col));
        vector<byte_t> v(bytes, bytes + kTrustLineAmountBytesCount);
        return bytesToTrustLineAmount(v);
    };

    TrustLineAmount incomingAmount = extractAmount(idx++);
    TrustLineAmount outgoingAmount = extractAmount(idx++);

    const unsigned char *balanceBytesPtr = reinterpret_cast<const unsigned char *>(PQgetvalue(res, 0, idx++));
    vector<byte_t> balanceBuf(balanceBytesPtr, balanceBytesPtr + kTrustLineBalanceSerializeBytesCount);
    TrustLineBalance balance = bytesToTrustLineBalance(balanceBuf);

    auto ownSignature = make_shared<Signature>(reinterpret_cast<const unsigned char *>(PQgetvalue(res, 0, idx++)));

    const unsigned char *csigPtr = reinterpret_cast<const unsigned char *>(PQgetvalue(res, 0, idx++));
    Signature::Shared contractorSignature = nullptr;
    if (csigPtr) {
        contractorSignature = make_shared<Signature>(csigPtr);
    }

    PQclear(res);
    return make_shared<AuditRecord>(
        number, incomingAmount, outgoingAmount, balance, ownSignature, contractorSignature);
}

const AuditNumber AuditHandlerPostgreSQL::getActualAuditNumber(
    TrustLineID trustLineID)
{
    const string query = "SELECT number FROM " + mTableName + " WHERE trust_line_id = $1 ORDER BY number DESC LIMIT 1;";
    const char *paramValues[1];
    int paramLengths[1] = {0};
    int paramFormats[1] = {0};
    string tlIdStr = to_string(trustLineID);
    paramValues[0] = tlIdStr.c_str();

    PGresult *res = PQexecParams(mDataBase, query.c_str(), 1, nullptr, paramValues, paramLengths, paramFormats, 0);
    checkResultTuples(mDataBase, res, "AuditHandlerPostgreSQL::getActualAuditNumber");

    if (PQntuples(res) == 0) {
        PQclear(res);
        throw NotFoundError("No records with requested trust line id");
    }
    AuditNumber number = static_cast<AuditNumber>(atoi(PQgetvalue(res, 0, 0)));
    PQclear(res);
    return number;
}

void AuditHandlerPostgreSQL::deleteRecords(
    TrustLineID trustLineID)
{
    const string query = "DELETE FROM " + mTableName + " WHERE trust_line_id = $1;";
    const char *paramValues[1];
    int paramLengths[1] = {0};
    int paramFormats[1] = {0};
    string tlIdStr = to_string(trustLineID);
    paramValues[0] = tlIdStr.c_str();

    PGresult *res = PQexecParams(mDataBase, query.c_str(), 1, nullptr, paramValues, paramLengths, paramFormats, 0);
    checkResultCmd(mDataBase, res, "AuditHandlerPostgreSQL::deleteRecords");
    PQclear(res);
}

void AuditHandlerPostgreSQL::deleteAuditByNumber(
    TrustLineID trustLineID,
    AuditNumber auditNumber)
{
    const string query = "DELETE FROM " + mTableName + " WHERE trust_line_id = $1 AND number = $2;";
    const char *paramValues[2];
    int paramLengths[2] = {0,0};
    int paramFormats[2] = {0,0};

    string tlIdStr = to_string(trustLineID);
    string numStr = to_string(auditNumber);
    paramValues[0] = tlIdStr.c_str();
    paramValues[1] = numStr.c_str();

    PGresult *res = PQexecParams(mDataBase, query.c_str(), 2, nullptr, paramValues, paramLengths, paramFormats, 0);
    checkResultCmd(mDataBase, res, "AuditHandlerPostgreSQL::deleteAuditByNumber");
    if (PQcmdTuples(res)[0] == '0') {
        PQclear(res);
        throw ValueError("No data were deleted");
    }
    PQclear(res);
}

vector<AuditRecord::Shared> AuditHandlerPostgreSQL::auditsLessEqualThanAuditNumber(
    TrustLineID trustLineID,
    AuditNumber auditNumber)
{
    const string query = "SELECT number, incoming_amount, outgoing_amount, balance, our_signature, contractor_signature FROM " + mTableName +
                         " WHERE trust_line_id = $1 AND number <= $2;";

    const char *paramValues[2];
    int paramLengths[2] = {0,0};
    int paramFormats[2] = {0,0};

    string tlIdStr = to_string(trustLineID);
    string numStr = to_string(auditNumber);
    paramValues[0] = tlIdStr.c_str();
    paramValues[1] = numStr.c_str();

    PGresult *res = PQexecParams(mDataBase, query.c_str(), 2, nullptr, paramValues, paramLengths, paramFormats, 1);
    checkResultTuples(mDataBase, res, "AuditHandlerPostgreSQL::auditsLessEqualThanAuditNumber");

    vector<AuditRecord::Shared> result;
    int rows = PQntuples(res);
    result.reserve(rows);

    for (int row = 0; row < rows; ++row) {
        int idx = 0;
        // Convert binary format to integer correctly
        AuditNumber number;
        const unsigned char *numberBytes = reinterpret_cast<const unsigned char *>(PQgetvalue(res, row, idx++));
        memcpy(&number, numberBytes, sizeof(AuditNumber));
        number = ntohl(number); // Convert from network byte order
        auto extractAmount = [&](int col)->TrustLineAmount {
            const unsigned char *bytes = reinterpret_cast<const unsigned char *>(PQgetvalue(res, row, col));
            vector<byte_t> v(bytes, bytes + kTrustLineAmountBytesCount);
            return bytesToTrustLineAmount(v);
        };
        TrustLineAmount incomingAmount = extractAmount(idx++);
        TrustLineAmount outgoingAmount = extractAmount(idx++);
        const unsigned char *balancePtr = reinterpret_cast<const unsigned char *>(PQgetvalue(res, row, idx++));
        vector<byte_t> balanceBuf(balancePtr, balancePtr + kTrustLineBalanceSerializeBytesCount);
        TrustLineBalance balance = bytesToTrustLineBalance(balanceBuf);
        auto ownSignature = make_shared<Signature>(reinterpret_cast<const unsigned char *>(PQgetvalue(res, row, idx++)));
        const unsigned char *csigPtr = reinterpret_cast<const unsigned char *>(PQgetvalue(res, row, idx++));
        Signature::Shared contractorSignature = nullptr;
        if (csigPtr) contractorSignature = make_shared<Signature>(csigPtr);

        result.push_back(make_shared<AuditRecord>(number, incomingAmount, outgoingAmount, balance, ownSignature, contractorSignature));
    }
    PQclear(res);
    return result;
}


LoggerStream AuditHandlerPostgreSQL::info() const
{
    return mLog.info(logHeader());
}

LoggerStream AuditHandlerPostgreSQL::warning() const
{
    return mLog.warning(logHeader());
}

const string AuditHandlerPostgreSQL::logHeader() const
{
    stringstream s;
    s << "[AuditHandlerPostgreSQL]";
    return s.str();
} 