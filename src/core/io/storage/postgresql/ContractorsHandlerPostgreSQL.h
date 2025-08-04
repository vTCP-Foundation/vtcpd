#ifndef VTCPD_CONTRACTORSHANDLERPOSTGRESQL_H
#define VTCPD_CONTRACTORSHANDLERPOSTGRESQL_H

#include "../interfaces/ContractorsHandler.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../contractors/Contractor.h"
#include "../../../common/memory/MemoryUtils.h"
#include <libpq-fe.h>
#include <vector>
#include <memory>

class ContractorsHandlerPostgreSQL : public ContractorsHandler
{
public:
    ContractorsHandlerPostgreSQL(
        PGconn *dbConnection,
        const std::string &tableName,
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

    std::vector<Contractor::Shared> allContractors() override;

    std::vector<ContractorID> allIDs() override;

    void removeContractor(
        ContractorID contractorID) override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const std::string logHeader() const;

    PGconn *mDataBase = nullptr;
    std::string mTableName;
    Logger &mLog;
};

#endif // VTCPD_CONTRACTORSHANDLERPOSTGRESQL_H 