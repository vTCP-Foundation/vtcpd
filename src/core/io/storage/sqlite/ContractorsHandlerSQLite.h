#ifndef VTCPD_CONTRACTORSHANDLERSQLITE_H
#define VTCPD_CONTRACTORSHANDLERSQLITE_H

#include "../../../logger/Logger.h"
#include "../interfaces/ContractorsHandler.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../common/memory/MemoryUtils.h"
#include <sqlite3.h>
#include <memory>

class ContractorsHandlerSQLite : public ContractorsHandler
{
public:
    ContractorsHandlerSQLite(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger &logger);

    void saveContractor(
        Contractor::Shared contractor) override;

    void saveContractorFull(
        Contractor::Shared contractor) override;

    void saveConfirmationInfo(
        Contractor::Shared contractor) override;

    void updateCryptoKey(
        Contractor::Shared contractor) override;

    void updateChannelIdOnContractorSide(
        Contractor::Shared contractor) override;

    vector<Contractor::Shared> allContractors() override;

    vector<ContractorID> allIDs() override;

    void removeContractor(
        ContractorID contractorID) override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const string logHeader() const;

    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger &mLog;
};

#endif //VTCPD_CONTRACTORSHANDLERSQLITE_H
