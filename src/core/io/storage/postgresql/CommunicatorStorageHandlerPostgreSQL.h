#ifndef VTCPD_COMMUNICATORSTORAGEHANDLERPOSTGRESQL_H
#define VTCPD_COMMUNICATORSTORAGEHANDLERPOSTGRESQL_H

#include "../interfaces/CommunicatorStorageHandler.h"
#include "CommunicatorMessagesQueueHandlerPostgreSQL.h"
#include "CommunicatorIOTransactionPostgreSQL.h"
#include <libpq-fe.h>
#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include <string>

class CommunicatorStorageHandlerPostgreSQL : public CommunicatorStorageHandler
{
public:
    CommunicatorStorageHandlerPostgreSQL(
        const std::string &connectionOptions,
        Logger &logger);
    ~CommunicatorStorageHandlerPostgreSQL();

    CommunicatorIOTransaction::Shared beginTransaction() override;
    CommunicatorIOTransaction::Unique beginTransactionUnique() override;

private:
    static PGconn* connection(
        const std::string &connectionOptions,
        Logger &logger);

    LoggerStream info() const;
    LoggerStream warning() const;
    const std::string logHeader() const;

    static PGconn *mDBConnection;
    Logger &mLog;

    const std::string kMessagesQueueTableName = "communicator_messages_queue";
    CommunicatorMessagesQueueHandlerPostgreSQL mCommunicatorMessagesQueueHandler;
    std::string mConnectionOptions;
};

#endif // VTCPD_COMMUNICATORSTORAGEHANDLERPOSTGRESQL_H 