#ifndef VTCPD_ADDRESSHANDLERSQLITE_H
#define VTCPD_ADDRESSHANDLERSQLITE_H

#include "../../../logger/Logger.h"
#include "../interfaces/AddressHandler.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../common/memory/MemoryUtils.h"
#include <sqlite3.h>
#include <vector>

class AddressHandlerSQLite : public AddressHandler
{
public:
    AddressHandlerSQLite(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger &logger);

    void saveAddress(
        const ContractorID contractorID,
        BaseAddress::Shared address);

    vector<BaseAddress::Shared> contractorAddresses(
        const ContractorID contractorID);

    void removeAddresses(
        const ContractorID contractorID);

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const string logHeader() const;

    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger &mLog;
};

#endif //VTCPD_ADDRESSHANDLERSQLITE_H
