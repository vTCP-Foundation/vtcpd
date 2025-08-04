#ifndef VTCPD_COMMUNICATORIOTRANSACTIONSQLITE_H
#define VTCPD_COMMUNICATORIOTRANSACTIONSQLITE_H

#include "../../../common/Types.h"
#include "../interfaces/CommunicatorIOTransaction.h"
#include "../interfaces/CommunicatorMessagesQueueHandler.h"
#include <sqlite3.h>

class CommunicatorIOTransactionSQLite : public CommunicatorIOTransaction
{
public:
    typedef shared_ptr<CommunicatorIOTransaction> Shared;
    typedef unique_ptr<CommunicatorIOTransaction> Unique;

    CommunicatorIOTransactionSQLite(
        sqlite3 *dbConnection,
        CommunicatorMessagesQueueHandler *communicatorMessagesQueueHandler,
        Logger &logger);

    ~CommunicatorIOTransactionSQLite();

    CommunicatorMessagesQueueHandler *communicatorMessagesQueueHandler();

    void rollback();

    void commit();

    void beginTransactionQuery();

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const string logHeader() const;

    sqlite3 *mDBConnection;
    CommunicatorMessagesQueueHandler *mCommunicatorMessagesQueueHandler;
    bool mIsTransactionBegin;
    Logger &mLog;
};

#endif //VTCPD_COMMUNICATORIOTRANSACTIONSQLITE_H
