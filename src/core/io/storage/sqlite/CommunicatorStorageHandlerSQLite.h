#ifndef VTCPD_COMMUNICATORSTORAGEHANDLERSQLITE_H
#define VTCPD_COMMUNICATORSTORAGEHANDLERSQLITE_H

#include "../../../logger/Logger.h"
#include "../interfaces/CommunicatorStorageHandler.h"
#include "../interfaces/CommunicatorMessagesQueueHandler.h"
#include "CommunicatorMessagesQueueHandlerSQLite.h"
#include "CommunicatorIOTransactionSQLite.h"
#include "../../../common/exceptions/IOError.h"
#include <sqlite3.h>
#include <boost/filesystem.hpp>
#include <vector>
namespace fs = boost::filesystem;
class CommunicatorStorageHandlerSQLite : public CommunicatorStorageHandler
{
public:
    CommunicatorStorageHandlerSQLite(
        const string &directory,
        const string &dataBaseName,
        Logger &logger);
    ~CommunicatorStorageHandlerSQLite();
    CommunicatorIOTransaction::Shared beginTransaction();
    CommunicatorIOTransaction::Unique beginTransactionUnique();
private:
    static void checkDirectory(
        const string &directory);
    static sqlite3* connection(
        const string &directory,
        const string &dataBaseName,
        Logger &logger);
    LoggerStream info() const;
    LoggerStream warning() const;
    const string logHeader() const;
    const string kMessagesQueueTableName = "communicator_messages_queue";
    static sqlite3 *mDBConnection;
    Logger &mLog;
    CommunicatorMessagesQueueHandlerSQLite mCommunicatorMessagesQueueHandler;
    string mDirectory;
    string mDataBaseName;
};
#endif //VTCPD_COMMUNICATORSTORAGEHANDLERSQLITE_H
