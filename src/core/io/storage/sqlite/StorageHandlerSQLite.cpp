#include "StorageHandlerSQLite.h"

sqlite3 *StorageHandlerSQLite::mDBConnection = nullptr;
string StorageHandlerSQLite::mCurrentDatabasePath = "";

StorageHandlerSQLite::StorageHandlerSQLite(
    const string &directory,
    const string &dataBaseName,
    Logger &logger):

    mDirectory(directory),
    mDataBaseName(dataBaseName),
    mContractorsHandler(connection(directory, dataBaseName, logger), kContractorsTableName, logger),
    mAddressHandler(connection(directory, dataBaseName, logger), kContractorAddressesTableName, logger),
    mTrustLineHandler(connection(directory, dataBaseName, logger), kTrustLineTableName, logger),
    mTransactionHandler(connection(directory, dataBaseName, logger), kTransactionTableName, logger),
    mHistoryStorage(connection(directory, dataBaseName, logger), kHistoryMainTableName, kHistoryAdditionalTableName, logger),
    mOwnKeysHandler(connection(directory, dataBaseName, logger), kOwnKeysTableName, logger),
    mContractorKeysHandler(connection(directory, dataBaseName, logger), kContractorKeysTableName, logger),
    mAuditHandler(connection(directory, dataBaseName, logger), kAuditTableName, logger),
    mIncomingPaymentReceiptHandler(connection(directory, dataBaseName, logger), kIncomingReceiptTableName, logger),
    mOutgoingPaymentReceiptHandler(connection(directory, dataBaseName, logger), kOutgoingReceiptTableName, logger),
    mPaymentTransactionsHandler(connection(directory, dataBaseName, logger), kPaymentTransactionsTableName, logger),
    mPaymentKeysHandler(connection(directory, dataBaseName, logger), kPaymentKeysTableName, logger),
    mPaymentParticipantsVotesHandler(connection(directory, dataBaseName, logger), kPaymentParticipantsVotesTableName, logger),
    mFeaturesHandler(connection(directory, dataBaseName, logger), kFeaturesTableName, logger),
    mLog(logger)
{
    sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);

    sqlite3_stmt *stmt;
    string query = "PRAGMA foreign_keys = ON;";
    int rc = sqlite3_prepare_v2( mDBConnection, query.c_str(), -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        throw IOError("StorageHandlerSQLite::enabling foreign keys: "
                      "Bad query; sqlite error: " + to_string(rc));
    }
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) {
    } else {
        throw IOError("StorageHandlerSQLite::enabling foreign keys: "
                      "Run query; sqlite error: " + to_string(rc));
    }
}

StorageHandlerSQLite::~StorageHandlerSQLite()
{
    if (mDBConnection != nullptr) {
        // Release any cached memory before closing the connection
        sqlite3_db_release_memory(mDBConnection);
        sqlite3_close_v2(mDBConnection);
    }
}

void StorageHandlerSQLite::checkDirectory(
    const string &directory)
{
    try {
        if (!fs::is_directory(fs::path(directory))) {
            fs::create_directories(
                fs::path(directory));
        }
    } catch (const fs::filesystem_error& e) {
        throw IOError(string("StorageHandlerSQLite::checkDirectory: ") + e.what());
    }
}

sqlite3* StorageHandlerSQLite::connection(
    const string &directory,
    const string &dataBaseName,
    Logger &logger)
{
    checkDirectory(directory);
    string dataBasePath = directory + "/" + dataBaseName;

    // If connection exists but path changed, close old connection
    if (mDBConnection != nullptr && mCurrentDatabasePath != dataBasePath) {
        sqlite3_close_v2(mDBConnection);
        mDBConnection = nullptr;
        mCurrentDatabasePath = "";
    }

    // If no connection, open new one
    if (mDBConnection == nullptr) {
        int rc = sqlite3_open_v2(dataBasePath.c_str(), &mDBConnection, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
        if (rc == SQLITE_OK) {
            mCurrentDatabasePath = dataBasePath;
        } else {
            throw IOError("StorageHandlerSQLite::connection "
                          "Can't open database " + dataBaseName);
        }
    }

    return mDBConnection;
}

IOTransaction::Shared StorageHandlerSQLite::beginTransaction()
{
    return make_shared<IOTransactionSQLite>(
               mDBConnection,
               &mTrustLineHandler,
               &mHistoryStorage,
               &mTransactionHandler,
               &mOwnKeysHandler,
               &mContractorKeysHandler,
               &mAuditHandler,
               &mIncomingPaymentReceiptHandler,
               &mOutgoingPaymentReceiptHandler,
               &mPaymentKeysHandler,
               &mPaymentParticipantsVotesHandler,
               &mPaymentTransactionsHandler,
               &mContractorsHandler,
               &mAddressHandler,
               &mFeaturesHandler,
               mLog);
}

void StorageHandlerSQLite::vacuum()
{
    sqlite3_db_config(mDBConnection, SQLITE_DBCONFIG_RESET_DATABASE, 1, 0);
    sqlite3_exec(mDBConnection, "VACUUM", 0, 0, 0);
    sqlite3_db_config(mDBConnection, SQLITE_DBCONFIG_RESET_DATABASE, 0, 0);
}

LoggerStream StorageHandlerSQLite::info() const
{
    return mLog.info(logHeader());
}

LoggerStream StorageHandlerSQLite::warning() const
{
    return mLog.warning(logHeader());
}

LoggerStream StorageHandlerSQLite::error() const
{
    return mLog.error(logHeader());
}

const string StorageHandlerSQLite::logHeader() const
{
    stringstream s;
    s << "StorageHandlerSQLite ";
    return s.str();
}