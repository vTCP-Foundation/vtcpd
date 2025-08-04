#ifndef VTCPD_TRUSTLINEHANDLERPOSTGRESQL_H
#define VTCPD_TRUSTLINEHANDLERPOSTGRESQL_H

#include "../interfaces/TrustLineHandler.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include <libpq-fe.h>
#include <vector>
#include <string>
#include <memory>

class TrustLineHandlerPostgreSQL : public TrustLineHandler
{
public:
    TrustLineHandlerPostgreSQL(
        PGconn *dbConnection,
        const std::string &tableName,
        Logger &logger);

    void saveTrustLine(
        TrustLine::Shared trustLine,
        const SerializedEquivalent equivalent) override;

    void updateTrustLineState(
        TrustLine::Shared trustLine,
        const SerializedEquivalent equivalent) override;

    void updateTrustLineIsContractorGateway(
        TrustLine::Shared trustLine,
        const SerializedEquivalent equivalent) override;

    std::vector<TrustLine::Shared> allTrustLinesByEquivalent(
        const SerializedEquivalent equivalent) override;

    void deleteTrustLine(
        ContractorID contractorID,
        const SerializedEquivalent equivalent) override;

    std::vector<SerializedEquivalent> equivalents() override;

    std::vector<TrustLineID> allIDs() override;

    std::vector<TrustLine::Shared> allTrustLinesByContractor(
        ContractorID contractorID) override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const std::string logHeader() const;

private:
    PGconn *mDataBase = nullptr;
    std::string mTableName;
    Logger &mLog;
};

#endif //VTCPD_TRUSTLINEHANDLERPOSTGRESQL_H 