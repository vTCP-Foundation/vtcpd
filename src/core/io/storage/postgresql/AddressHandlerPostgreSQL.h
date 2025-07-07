#ifndef VTCPD_ADDRESSHANDLERPOSTGRESQL_H
#define VTCPD_ADDRESSHANDLERPOSTGRESQL_H

#include "../interfaces/AddressHandler.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../common/memory/MemoryUtils.h"
#include "../../../contractors/addresses/BaseAddress.h"
#include "../../../contractors/addresses/IPv4WithPortAddress.h"
#include "../../../contractors/addresses/GNSAddress.h"

#include <libpq-fe.h>
#include <vector>

// PostgreSQL implementation of AddressHandler interface.
// NOTE: Single-threaded design implies one static PG connection shared across
// all handler instances (provided externally by StorageHandlerPostgreSQL).

class AddressHandlerPostgreSQL : public AddressHandler
{
public:
    AddressHandlerPostgreSQL(
        PGconn *dbConnection,
        const std::string &tableName,
        Logger &logger);

    void saveAddress(
        ContractorID contractorID,
        BaseAddress::Shared address) override;

    std::vector<BaseAddress::Shared> contractorAddresses(
        ContractorID contractorID) override;

    void removeAddresses(
        ContractorID contractorID) override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const std::string logHeader() const;

    PGconn *mDataBase = nullptr;
    std::string mTableName;
    Logger &mLog;
};

#endif // VTCPD_ADDRESSHANDLERPOSTGRESQL_H 