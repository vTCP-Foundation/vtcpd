#include "CommunicatorStorageHandlerSQLite.h"
#include "CommunicatorIOTransactionSQLite.h"

sqlite3 *CommunicatorStorageHandlerSQLite::mDBConnection = nullptr;

CommunicatorStorageHandlerSQLite::CommunicatorStorageHandlerSQLite(
    const string &directory,
    const string &dataBaseName,
    Logger &logger):

    mDirectory(directory),
    mDataBaseName(dataBaseName),
    mCommunicatorMessagesQueueHandler(connection(directory, dataBaseName, logger), kMessagesQueueTableName, logger),
    mLog(logger)
{
    sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);
}

CommunicatorStorageHandlerSQLite::~CommunicatorStorageHandlerSQLite()
{
    if (mDBConnection != nullptr) {
        sqlite3_close_v2(mDBConnection);
    }
}

void CommunicatorStorageHandlerSQLite::checkDirectory(
    const string &directory)
{
    if (!fs::is_directory(fs::path(directory))) {
        fs::create_directories(
            fs::path(directory));
    }
}

sqlite3* CommunicatorStorageHandlerSQLite::connection(
    const string &directory,
    const string &dataBaseName,
    Logger &logger)
{
    checkDirectory(directory);
    if (mDBConnection != nullptr)
        return mDBConnection;
    string dataBasePath = directory + "/" + dataBaseName;
    int rc = sqlite3_open_v2(dataBasePath.c_str(), &mDBConnection, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc == SQLITE_OK) {
    } else {
        throw IOError("CommunicatorStorageHandlerSQLite::connection "
                      "Can't open database " + dataBaseName);
    }
    return mDBConnection;
}

CommunicatorIOTransaction::Shared CommunicatorStorageHandlerSQLite::beginTransaction()
{
    return make_shared<CommunicatorIOTransactionSQLite>(
               mDBConnection,
               &mCommunicatorMessagesQueueHandler,
               mLog);
}

CommunicatorIOTransaction::Unique CommunicatorStorageHandlerSQLite::beginTransactionUnique()
{
    return make_unique<CommunicatorIOTransactionSQLite>(
               mDBConnection,
               &mCommunicatorMessagesQueueHandler,
               mLog);
}

LoggerStream CommunicatorStorageHandlerSQLite::info() const
{
    return mLog.info(logHeader());
}

LoggerStream CommunicatorStorageHandlerSQLite::warning() const
{
    return mLog.warning(logHeader());
}

const string CommunicatorStorageHandlerSQLite::logHeader() const
{
    stringstream s;
    s << "[CommunicatorStorageHandler]";
    return s.str();
}
