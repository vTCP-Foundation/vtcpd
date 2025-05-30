#include "FeaturesHandler.h"

FeaturesHandler::FeaturesHandler(
    sqlite3 *dbConnection,
    const string &tableName,
    Logger &logger) :

    mDataBase(dbConnection),
    mTableName(tableName),
    mLog(logger)
{
    // Validate input parameters.
    if (dbConnection == nullptr) {
        throw ValueError("FeaturesHandler::constructor: Database connection cannot be null.");
    }

    if (tableName.empty()) {
        throw ValueError("FeaturesHandler::constructor: Table name cannot be empty.");
    }

    // Create the main table
    string query = "CREATE TABLE IF NOT EXISTS " + mTableName +
                   " (feature_name STRING NOT NULL, "
                   "feature_length INTEGER NOT NULL, "
                   "feature_value STRING NOT NULL);";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());
    int rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("FeaturesHandler::constructor: Failed to create table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Create unique index on feature_name
    query = "CREATE UNIQUE INDEX IF NOT EXISTS " + mTableName + "_feature_name_idx on " + mTableName + " (feature_name);";
    SQLiteStatementRAII indexStmt(mDataBase, query.c_str());
    rc = sqlite3_step(indexStmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("FeaturesHandler::constructor: Failed to create unique index on table '" + mTableName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "FeaturesHandler initialized: table=" << mTableName;
#endif
}

void FeaturesHandler::saveFeature(
    const string &featureName,
    const string &featureValue)
{
    if (featureName.empty()) {
        throw ValueError("FeaturesHandler::saveFeature: Feature name cannot be empty.");
    }

    string query = "INSERT OR REPLACE INTO " + mTableName +
                   " (feature_name, feature_length, feature_value) "
                   "VALUES (?, ?, ?);";

    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    // Bind feature name
    int rc = sqlite3_bind_text(stmt.get(), 1, featureName.c_str(), (int)featureName.size(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("FeaturesHandler::saveFeature: Failed to bind feature_name. "
                      "FeatureName='" + featureName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Bind feature length
    rc = sqlite3_bind_int(stmt.get(), 2, (int)featureValue.size());
    if (rc != SQLITE_OK) {
        throw IOError("FeaturesHandler::saveFeature: Failed to bind feature_length. "
                      "FeatureName='" + featureName + "', FeatureLength=" + to_string(featureValue.size()) + ". "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Bind feature value
    rc = sqlite3_bind_text(stmt.get(), 3, featureValue.c_str(), (int)featureValue.size(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("FeaturesHandler::saveFeature: Failed to bind feature_value. "
                      "FeatureName='" + featureName + "', FeatureLength=" + to_string(featureValue.size()) + ". "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Execute the statement
    rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw IOError("FeaturesHandler::saveFeature: Failed to execute INSERT OR REPLACE. "
                      "FeatureName='" + featureName + "', FeatureLength=" + to_string(featureValue.size()) + ". "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

#ifdef STORAGE_HANDLER_DEBUG_LOG
    info() << "Feature saved: FeatureName='" << featureName
           << "', FeatureLength=" << featureValue.size();
#endif
}

string FeaturesHandler::getFeature(
    const string &featureName)
{
    if (featureName.empty()) {
        throw ValueError("FeaturesHandler::getFeature: Feature name cannot be empty.");
    }

    string query = "SELECT feature_length, feature_value FROM " + mTableName + " WHERE feature_name = ?;";
    SQLiteStatementRAII stmt(mDataBase, query.c_str());

    // Bind feature name
    int rc = sqlite3_bind_text(stmt.get(), 1, featureName.c_str(), (int)featureName.size(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        throw IOError("FeaturesHandler::getFeature: Failed to bind feature_name. "
                      "FeatureName='" + featureName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }

    // Execute the query
    rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        auto featureSize = (size_t)sqlite3_column_int(stmt.get(), 0);
        auto featureValueBytes = (byte_t*)sqlite3_column_blob(stmt.get(), 1);

        if (featureValueBytes == nullptr && featureSize > 0) {
            throw IOError("FeaturesHandler::getFeature: Retrieved null feature value with non-zero size. "
                          "FeatureName='" + featureName + "', ExpectedSize=" + to_string(featureSize) + ".");
        }

        string result(reinterpret_cast<char const*>(featureValueBytes), featureSize);

#ifdef STORAGE_HANDLER_DEBUG_LOG
        info() << "Feature retrieved: FeatureName='" << featureName
               << "', FeatureLength=" << featureSize;
#endif
        return result;
    } else if (rc == SQLITE_DONE) {
        throw NotFoundError("FeaturesHandler::getFeature: Feature not found. "
                            "FeatureName='" + featureName + "'.");
    } else {
        throw IOError("FeaturesHandler::getFeature: Failed to execute SELECT. "
                      "FeatureName='" + featureName + "'. "
                      "SQLite error: " + to_string(rc) + " (" + sqlite3_errmsg(mDataBase) + ").");
    }
}

LoggerStream FeaturesHandler::info() const
{
    return mLog.info(logHeader());
}

LoggerStream FeaturesHandler::warning() const
{
    return mLog.warning(logHeader());
}

const string FeaturesHandler::logHeader() const
{
    stringstream s;
    s << "[FeaturesHandler: (" << mTableName << ")]";
    return s.str();
}