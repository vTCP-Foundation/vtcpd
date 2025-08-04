#ifndef VTCPD_FEATURESHANDLERSQLITE_H
#define VTCPD_FEATURESHANDLERSQLITE_H

#include "../../../logger/Logger.h"
#include "../interfaces/FeaturesHandler.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../common/memory/MemoryUtils.h"
#include "SQLiteStatementRAII.h"
#include <sqlite3.h>
#include <memory>

/**
 * Handler for managing enabled features in SQLite database.
 *
 * This class provides persistent storage for enabled features using SQLite.
 * Features are stored as key-value pairs where both key and value are strings.
 */
class FeaturesHandlerSQLite : public FeaturesHandler
{
public:
    /**
     * Constructs a FeaturesHandler for managing features in SQLite database.
     *
     * Creates the features table if it doesn't exist with schema:
     * - feature_name: STRING NOT NULL (feature identifier)
     * - feature_length: INTEGER NOT NULL (length of feature value)
     * - feature_value: STRING NOT NULL (the actual feature data)
     * Also creates a unique index on feature_name column.
     * @param dbConnection SQLite database connection (must not be null)
     * @param tableName Name of the table to create/use (must not be empty)
     * @param logger Logger instance for debugging and error reporting
     * @throws ValueError if dbConnection is null or tableName is empty
     * @throws IOError if database operations fail
     */
    FeaturesHandlerSQLite(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger &logger);

    /**
     * Saves or updates a feature in the database.
     */
    void saveFeature(
        const string &featureName,
        const string &featureValue) override;

    /**
     * Retrieves a feature value by its name.
     */
    string getFeature(
        const string &featureName) override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const string logHeader() const;

    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger &mLog;
};

#endif //VTCPD_FEATURESHANDLERSQLITE_H
