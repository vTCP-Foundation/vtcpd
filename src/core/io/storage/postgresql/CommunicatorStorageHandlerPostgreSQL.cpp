#include "CommunicatorStorageHandlerPostgreSQL.h"
#include <sstream>

PGconn* CommunicatorStorageHandlerPostgreSQL::mDBConnection = nullptr;

using namespace std;

namespace {
inline void checkConn(PGconn *c, const string &prefix){ if(PQstatus(c)!=CONNECTION_OK){ throw IOError(prefix+": "+string(PQerrorMessage(c))); }}
}

CommunicatorStorageHandlerPostgreSQL::CommunicatorStorageHandlerPostgreSQL(
    const string &connectionOptions,
    Logger &logger):
    mLog(logger),
    mCommunicatorMessagesQueueHandler(connection(connectionOptions, logger), kMessagesQueueTableName, logger),
    mConnectionOptions(connectionOptions)
{
}

CommunicatorStorageHandlerPostgreSQL::~CommunicatorStorageHandlerPostgreSQL()
{
    if (mDBConnection){ PQfinish(mDBConnection); mDBConnection=nullptr; }
}

PGconn* CommunicatorStorageHandlerPostgreSQL::connection(const string &options, Logger &logger){
    if (mDBConnection) return mDBConnection;
    mDBConnection = PQconnectdb(options.c_str());
    checkConn(mDBConnection, "CommunicatorStorageHandlerPostgreSQL::connection");
    return mDBConnection;
}

CommunicatorIOTransaction::Shared CommunicatorStorageHandlerPostgreSQL::beginTransaction(){
    return make_shared<CommunicatorIOTransactionPostgreSQL>(
        mDBConnection,
        &mCommunicatorMessagesQueueHandler,
        mLog);
}

CommunicatorIOTransaction::Unique CommunicatorStorageHandlerPostgreSQL::beginTransactionUnique(){
    return make_unique<CommunicatorIOTransactionPostgreSQL>(
        mDBConnection,
        &mCommunicatorMessagesQueueHandler,
        mLog);
}

LoggerStream CommunicatorStorageHandlerPostgreSQL::info() const { return mLog.info(logHeader()); }
LoggerStream CommunicatorStorageHandlerPostgreSQL::warning() const { return mLog.warning(logHeader()); }
const string CommunicatorStorageHandlerPostgreSQL::logHeader() const { stringstream s; s << "[CommunicatorStorageHandlerPG]"; return s.str(); } 