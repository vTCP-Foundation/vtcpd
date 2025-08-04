#include "CommunicatorIOTransactionPostgreSQL.h"
#include <sstream>

using namespace std;

namespace {
inline void checkCmd(PGconn *db, PGresult *res, const string &prefix) {
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        string err = PQerrorMessage(db);
        PQclear(res);
        throw IOError(prefix + ": " + err);
    }
}
}

CommunicatorIOTransactionPostgreSQL::CommunicatorIOTransactionPostgreSQL(
    PGconn *dbConnection,
    CommunicatorMessagesQueueHandler *communicatorMessagesQueueHandler,
    Logger &logger):
    mDBConnection(dbConnection),
    mCommunicatorMessagesQueueHandler(communicatorMessagesQueueHandler),
    mIsTransactionBegin(true),
    mLog(logger)
{
    beginTransactionQuery();
}

CommunicatorIOTransactionPostgreSQL::~CommunicatorIOTransactionPostgreSQL()
{
    commit();
}

#define ENSURE_BEGIN if(!mIsTransactionBegin) throw IOError("CommunicatorIOTransactionPostgreSQL: transaction finished");

CommunicatorMessagesQueueHandler* CommunicatorIOTransactionPostgreSQL::communicatorMessagesQueueHandler()
{
    ENSURE_BEGIN
    return mCommunicatorMessagesQueueHandler;
}

void CommunicatorIOTransactionPostgreSQL::commit()
{
    if (!mIsTransactionBegin) return;
    PGresult *res = PQexec(mDBConnection, "COMMIT;");
    checkCmd(mDBConnection, res, "CommunicatorIOTransactionPostgreSQL::commit");
    PQclear(res);
    mIsTransactionBegin = false;
}

void CommunicatorIOTransactionPostgreSQL::rollback()
{
    PGresult *res = PQexec(mDBConnection, "ROLLBACK;");
    checkCmd(mDBConnection, res, "CommunicatorIOTransactionPostgreSQL::rollback");
    PQclear(res);
    mIsTransactionBegin = false;
}

void CommunicatorIOTransactionPostgreSQL::beginTransactionQuery()
{
    PGresult *res = PQexec(mDBConnection, "BEGIN;");
    checkCmd(mDBConnection, res, "CommunicatorIOTransactionPostgreSQL::begin");
    PQclear(res);
}

LoggerStream CommunicatorIOTransactionPostgreSQL::info() const { return mLog.info(logHeader()); }
LoggerStream CommunicatorIOTransactionPostgreSQL::warning() const { return mLog.warning(logHeader()); }
const string CommunicatorIOTransactionPostgreSQL::logHeader() const { stringstream s; s << "[CommunicatorIOTransactionPG]"; return s.str(); } 