#ifndef VTCPD_FEATURESHANDLERPOSTGRESQL_H
#define VTCPD_FEATURESHANDLERPOSTGRESQL_H

#include "../interfaces/FeaturesHandler.h"
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../common/memory/MemoryUtils.h"
#include <libpq-fe.h>
#include <string>

class FeaturesHandlerPostgreSQL : public FeaturesHandler
{
public:
    FeaturesHandlerPostgreSQL(
        PGconn *dbConnection,
        const std::string &tableName,
        Logger &logger);

    void saveFeature(
        const std::string &featureName,
        const std::string &featureValue) override;

    std::string getFeature(
        const std::string &featureName) override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const std::string logHeader() const;

    PGconn *mDataBase = nullptr;
    std::string mTableName;
    Logger &mLog;
};

#endif // VTCPD_FEATURESHANDLERPOSTGRESQL_H 