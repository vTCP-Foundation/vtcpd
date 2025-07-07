#ifndef VTCPD_COMMUNICATORIOTRANSACTIONPOSTGRESQL_H
#define VTCPD_COMMUNICATORIOTRANSACTIONPOSTGRESQL_H

#include "../interfaces/CommunicatorIOTransaction.h"
#include "CommunicatorMessagesQueueHandlerPostgreSQL.h"
#include <libpq-fe.h>
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"

class CommunicatorIOTransactionPostgreSQL : public CommunicatorIOTransaction
{
public:
    CommunicatorIOTransactionPostgreSQL(
        PGconn *dbConnection,
        CommunicatorMessagesQueueHandler *communicatorMessagesQueueHandler,
        Logger &logger);
    ~CommunicatorIOTransactionPostgreSQL();

    CommunicatorMessagesQueueHandler *communicatorMessagesQueueHandler() override;

    void rollback() override;

    void commit();
    void beginTransactionQuery();

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const std::string logHeader() const;

    PGconn *mDBConnection;
    CommunicatorMessagesQueueHandler *mCommunicatorMessagesQueueHandler;
    bool mIsTransactionBegin;
    Logger &mLog;
};

#endif // VTCPD_COMMUNICATORIOTRANSACTIONPOSTGRESQL_H 