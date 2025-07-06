#include "AddressHandlerSQLite.h"

AddressHandlerSQLite::AddressHandlerSQLite(
    sqlite3 *dbConnection,
    const string &tableName,
    Logger &logger) :

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   "(type INTEGER NOT NULL, "
                   "contractor_id INTEGER NOT NULL, "
                   "address_size INTEGER NOT NULL, "
                   "address BLOB NOT NULL, "
                   "FOREIGN KEY(contractor_id) REFERENCES contractors(id) ON DELETE CASCADE ON UPDATE CASCADE);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("AddressHandlerSQLite::creating table: "
                      "Bad query; sqlite error: " +
                      to_string(rc));
    }

    query = "CREATE INDEX IF NOT EXISTS " + mTableName + "_contractor_id on " + mTableName + "(contractor_id);";
    SQLiteStatementRAII stmtIdx(mDataBase, query.c_str());
    rc = sqlite3_step(stmtIdx.get());
    if (rc != SQLITE_DONE) {
        throw IOError("AddressHandlerSQLite::creating index for ContractorID: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "AddressHandler initialized: table=" << mTableName;
#endif
}

void AddressHandlerSQLite::saveAddress(
    ContractorID contractorID,
    BaseAddress::Shared address)
{
    string query = "INSERT INTO " + mTableName +
                   "(type, contractor_id, address_size, address) "
                   "VALUES (?, ?, ?, ?);";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_int(stmt.get(), 1, (int)address->typeID());
    if (rc != SQLITE_OK) {
        throw IOError("AddressHandlerSQLite::saveAddress: "
                      "Bad binding of address type; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 2, contractorID);
    if (rc != SQLITE_OK) {
        throw IOError("AddressHandlerSQLite::saveAddress: "
                      "Bad binding of Contractor ID; sqlite error: " +
                      to_string(rc));
    }
    rc = sqlite3_bind_int(stmt.get(), 3, (int)address->serializedSize());
    if (rc != SQLITE_OK) {
        throw IOError("AddressHandlerSQLite::saveAddress: "
                      "Bad binding of address size; sqlite error: " +
                      to_string(rc));
    }
    auto serializedAddress = address->serializeToBytes();
    rc = sqlite3_bind_blob(stmt.get(), 4, serializedAddress.get(), (int)address->serializedSize(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("TrustLineHandler::saveTrustLine: "
                      "Bad binding of address; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "prepare inserting is completed successfully";
#endif
    } else {
        throw IOError("AddressHandlerSQLite::saveAddress: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }
}

vector<BaseAddress::Shared> AddressHandlerSQLite::contractorAddresses(
    ContractorID contractorID)
{
    vector<BaseAddress::Shared> result;
    string query = "SELECT type, address_size, address FROM " + mTableName + " WHERE contractor_id = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_int(stmt.get(), 1, contractorID);
    if (rc != SQLITE_OK) {
        throw IOError("AddressHandlerSQLite::contractorAddresses: "
                      "Bad binding of Contractor ID; sqlite error: " +
                      to_string(rc));
    }
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        auto addressType = (BaseAddress::AddressType)sqlite3_column_int(stmt.get(), 0);
        auto addressSize = (size_t)sqlite3_column_int(stmt.get(), 1);
        auto addressBytes = (byte_t*)sqlite3_column_blob(stmt.get(), 2);
        try {
            switch (addressType) {
            case BaseAddress::IPv4_IncludingPort: {
                result.push_back(
                    make_shared<IPv4WithPortAddress>(
                        addressBytes));
                break;
            }
            case BaseAddress::GNS: {
                result.push_back(
                    make_shared<GNSAddress>(
                        addressBytes));
                break;
            }
            default: {
                throw ValueError("AddressHandlerSQLite::contractorAddresses: "
                                 "Invalid address type: " +
                                 to_string(addressType));
            }
            }
        } catch (std::exception &e) {
            throw Exception("AddressHandlerSQLite::contractorAddresses. "
                            "Unable to create address instance from DB of type " +
                            to_string(addressType) + " Details: " + e.what());
        } catch (...) {
            throw Exception("AddressHandlerSQLite::contractorAddresses. "
                            "Unable to create address instance from DB of type " +
                            to_string(addressType));
        }
    }
    return result;
}

void AddressHandlerSQLite::removeAddresses(
    ContractorID contractorID)
{
    string query = "DELETE FROM " + mTableName + " WHERE contractor_id = ?";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_bind_int(stmt.get(), 1, contractorID);
    if (rc != SQLITE_OK) {
        throw IOError("AddressHandlerSQLite::removeAddresses: "
                      "Bad binding of ContractorID; sqlite error: " +
                      to_string(rc));
    }

    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "deleting is completed successfully";
#endif
    } else {
        throw IOError("AddressHandlerSQLite::removeAddresses: "
                      "Run query; sqlite error: " +
                      to_string(rc));
    }
}

LoggerStream AddressHandlerSQLite::info() const
{
    return mLog.info(logHeader());
}

LoggerStream AddressHandlerSQLite::warning() const
{
    return mLog.warning(logHeader());
}

const string AddressHandlerSQLite::logHeader() const
{
    stringstream s;
    s << "[AddressHandler]";
    return s.str();
}