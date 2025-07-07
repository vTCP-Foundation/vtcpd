#include "AddressHandlerPostgreSQL.h"

#include <sstream>

using namespace std;

AddressHandlerPostgreSQL::AddressHandlerPostgreSQL(
    PGconn *dbConnection,
    const string &tableName,
    Logger &logger) :

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    if (mDataBase == nullptr) {
        throw IOError("AddressHandlerPostgreSQL: Database connection is null");
    }

    // Create main table
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   "(type INTEGER NOT NULL, "
                   "contractor_id INTEGER NOT NULL, "
                   "address_size INTEGER NOT NULL, "
                   "address BYTEA NOT NULL, "
                   "FOREIGN KEY(contractor_id) REFERENCES contractors(id) "
                   "ON DELETE CASCADE ON UPDATE CASCADE);";

    PGresult *res = PQexec(mDataBase, query.c_str());
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        string err = PQerrorMessage(mDataBase);
        PQclear(res);
        throw IOError("AddressHandlerPostgreSQL::creating table: " + err);
    }
    PQclear(res);

    // Create index on contractor_id
    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_contractor_id on " + mTableName + "(contractor_id);";
    res = PQexec(mDataBase, query.c_str());
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        string err = PQerrorMessage(mDataBase);
        PQclear(res);
        throw IOError("AddressHandlerPostgreSQL::creating index for ContractorID: " + err);
    }
    PQclear(res);

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "AddressHandler initialized: table=" << mTableName;
#endif
}

void AddressHandlerPostgreSQL::saveAddress(
    ContractorID contractorID,
    BaseAddress::Shared address)
{
    const string query = "INSERT INTO " + mTableName +
                         "(type, contractor_id, address_size, address) "
                         "VALUES ($1, $2, $3, $4);";

    // Serialize parameters
    const auto addressBytes = address->serializeToBytes();

    const int kParams = 4;
    const char *paramValues[kParams];
    int paramLengths[kParams];
    int paramFormats[kParams];

    string typeStr = to_string(static_cast<int>(address->typeID()));
    string contractorIdStr = to_string(contractorID);
    string sizeStr = to_string(address->serializedSize());

    paramValues[0] = typeStr.c_str();
    paramValues[1] = contractorIdStr.c_str();
    paramValues[2] = sizeStr.c_str();
    paramValues[3] = reinterpret_cast<const char *>(addressBytes.get());

    paramLengths[0] = 0; // text
    paramLengths[1] = 0;
    paramLengths[2] = 0;
    paramLengths[3] = static_cast<int>(address->serializedSize());

    paramFormats[0] = 0; // text
    paramFormats[1] = 0;
    paramFormats[2] = 0;
    paramFormats[3] = 1; // binary for BYTEA

    PGresult *res = PQexecParams(
        mDataBase,
        query.c_str(),
        kParams,
        nullptr,
        paramValues,
        paramLengths,
        paramFormats,
        0 /* text results */);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        string err = PQerrorMessage(mDataBase);
        PQclear(res);
        throw IOError("AddressHandlerPostgreSQL::saveAddress: " + err);
    }
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "prepare inserting is completed successfully";
#endif
}

vector<BaseAddress::Shared> AddressHandlerPostgreSQL::contractorAddresses(
    ContractorID contractorID)
{
    vector<BaseAddress::Shared> result;

    const string query = "SELECT type, address_size, address FROM " + mTableName + " WHERE contractor_id = $1;";

    const char *paramValues[1];
    int paramLengths[1];
    int paramFormats[1];

    string contractorIdStr = to_string(contractorID);
    paramValues[0] = contractorIdStr.c_str();
    paramLengths[0] = 0;
    paramFormats[0] = 0;

    PGresult *res = PQexecParams(
        mDataBase,
        query.c_str(),
        1,
        nullptr,
        paramValues,
        paramLengths,
        paramFormats,
        1 /* binary results for BYTEA */);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        string err = PQerrorMessage(mDataBase);
        PQclear(res);
        throw IOError("AddressHandlerPostgreSQL::contractorAddresses: " + err);
    }

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        int addressType = atoi(PQgetvalue(res, i, 0));
        size_t addressSize = static_cast<size_t>(atoi(PQgetvalue(res, i, 1)));
        const unsigned char *addressBytes = reinterpret_cast<const unsigned char *>(PQgetvalue(res, i, 2));

        try {
            switch (static_cast<BaseAddress::AddressType>(addressType)) {
                case BaseAddress::IPv4_IncludingPort: {
                    result.push_back(make_shared<IPv4WithPortAddress>(addressBytes));
                    break;
                }
                case BaseAddress::GNS: {
                    result.push_back(make_shared<GNSAddress>(addressBytes));
                    break;
                }
                default: {
                    throw ValueError("AddressHandlerPostgreSQL::contractorAddresses: Invalid address type: " + to_string(addressType));
                }
            }
        } catch (std::exception &e) {
            PQclear(res);
            throw Exception("AddressHandlerPostgreSQL::contractorAddresses. Unable to create address instance. Details: " + string(e.what()));
        } catch (...) {
            PQclear(res);
            throw Exception("AddressHandlerPostgreSQL::contractorAddresses. Unable to create address instance.");
        }
    }

    PQclear(res);
    return result;
}

void AddressHandlerPostgreSQL::removeAddresses(
    ContractorID contractorID)
{
    const string query = "DELETE FROM " + mTableName + " WHERE contractor_id = $1;";

    const char *paramValues[1];
    int paramLengths[1];
    int paramFormats[1];

    string contractorIdStr = to_string(contractorID);
    paramValues[0] = contractorIdStr.c_str();
    paramLengths[0] = 0;
    paramFormats[0] = 0;

    PGresult *res = PQexecParams(
        mDataBase,
        query.c_str(),
        1,
        nullptr,
        paramValues,
        paramLengths,
        paramFormats,
        0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        string err = PQerrorMessage(mDataBase);
        PQclear(res);
        throw IOError("AddressHandlerPostgreSQL::removeAddresses: " + err);
    }
    PQclear(res);
#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "deleting is completed successfully";
#endif
}

LoggerStream AddressHandlerPostgreSQL::info() const
{
    return mLog.info(logHeader());
}

LoggerStream AddressHandlerPostgreSQL::warning() const
{
    return mLog.warning(logHeader());
}

const string AddressHandlerPostgreSQL::logHeader() const
{
    stringstream s;
    s << "[AddressHandlerPostgreSQL]";
    return s.str();
} 