#ifndef VTCPD_COMMUNICATORMESSAGESQUEUEHANDLERPOSTGRESQL_H
#define VTCPD_COMMUNICATORMESSAGESQUEUEHANDLERPOSTGRESQL_H

#include "../interfaces/CommunicatorMessagesQueueHandler.h"
#include "../../../logger/Logger.h"
#include "../../../common/Types.h"
#include "../../../network/messages/Message.hpp"
#include "../../../transactions/transactions/base/TransactionUUID.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/memory/MemoryUtils.h"
#include <libpq-fe.h>
#include <tuple>
#include <vector>
#include <string>

class CommunicatorMessagesQueueHandlerPostgreSQL : public CommunicatorMessagesQueueHandler
{
public:
    CommunicatorMessagesQueueHandlerPostgreSQL(
        PGconn *dbConnection,
        const std::string &tableName,
        Logger &logger);

    void saveRecord(
        ContractorID contractorID,
        const SerializedEquivalent equivalent,
        const TransactionUUID &transactionUUID,
        const Message::SerializedType messageType,
        BytesShared message,
        size_t messageBytesCount) override;

    std::vector<std::tuple<ContractorID, BytesShared, Message::SerializedType>> allMessages() override;

    void deleteRecord(
        ContractorID contractorID,
        const SerializedEquivalent equivalent,
        const Message::SerializedType messageType) override;

    void deleteRecord(
        ContractorID contractorID,
        const TransactionUUID &transactionUUID) override;

private:
    LoggerStream info() const;
    LoggerStream warning() const;
    const std::string logHeader() const;

    PGconn *mDataBase = nullptr;
    std::string mTableName;
    Logger &mLog;
};

#endif // VTCPD_COMMUNICATORMESSAGESQUEUEHANDLERPOSTGRESQL_H 