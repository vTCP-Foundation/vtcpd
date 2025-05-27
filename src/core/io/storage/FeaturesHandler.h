#ifndef VTCPD_FEATURESHANDLER_H
#define VTCPD_FEATURESHANDLER_H

#include "../../common/Types.h"
#include "../../logger/Logger.h"
#include "../../common/memory/MemoryUtils.h"
#include "../../common/exceptions/IOError.h"
#include "../../common/exceptions/NotFoundError.h"
#include "../../common/exceptions/ValueError.h"
#include "SQLiteStatementRAII.h"

#include <sqlite3.h>
#include <string>

/**
 * Handler for managing enabled features in SQLite database.
 *
 * This class provides persistent storage for enabled features using SQLite.
 * Features are stored as key-value pairs where both key and value are strings.
 */
class FeaturesHandler
{

public:
    /**
     * Constructs a FeaturesHandler for managing features in SQLite database.
     *
     * Creates the features table if it doesn't exist with schema:
     * - feature_name: STRING NOT NULL (feature identifier)
     * - feature_length: INTEGER NOT NULL (length of feature value)
     * - feature_value: STRING NOT NULL (the actual feature data)
     *
     * Also creates a unique index on feature_name column.
     *
     * @param dbConnection SQLite database connection (must not be null)
     * @param tableName Name of the table to create/use (must not be empty)
     * @param logger Logger instance for debugging and error reporting
     *
     * @throws ValueError if dbConnection is null or tableName is empty
     * @throws IOError if database operations fail
     */
    FeaturesHandler(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger &logger);

    /**
     * Saves or updates a feature in the database.
     *
     * Uses INSERT OR REPLACE to either insert a new feature or update an existing one.
     * The feature length is automatically calculated from the feature value.
     *
     * @param featureName Name/key of the feature (must not be empty)
     * @param featureValue Value of the feature to store
     * @throws ValueError if featureName is empty
     * @throws IOError if database operation fails
     */
    void saveFeature(
        const string& featureName,
        const string& featureValue);

    /**
     * Retrieves a feature value by its name.
     *
     * @param featureName Name/key of the feature to retrieve (must not be empty)
     * @return The feature value as a string
     * @throws ValueError if featureName is empty
     * @throws NotFoundError if feature not found
     * @throws IOError if database operation fails
     */
    string getFeature(
        const string& featureName);

private:
    LoggerStream info() const;

    LoggerStream warning() const;

    const string logHeader() const;

private:
    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger &mLog;
};


#endif //VTCPD_FEATURESHANDLER_H
