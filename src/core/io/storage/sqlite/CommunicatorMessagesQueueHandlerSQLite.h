#ifndef VTCPD_COMMUNICATORMESSAGESQUEUEHANDLERSQLITE_H
#define VTCPD_COMMUNICATORMESSAGESQUEUEHANDLERSQLITE_H

#include "../../../logger/Logger.h"
#include "../interfaces/CommunicatorMessagesQueueHandler.h"
#include "../../../common/Types.h"
#include "../../../network/messages/Message.hpp"
#include "../../../transactions/transactions/base/TransactionUUID.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/memory/MemoryUtils.h"
#include <sqlite3.h>
#include <tuple>
class CommunicatorMessagesQueueHandlerSQLite : public CommunicatorMessagesQueueHandler
{
public:
    CommunicatorMessagesQueueHandlerSQLite(
        sqlite3 *dbConnection,
        const string &tableName,
        Logger &logger);
    void saveRecord(
        ContractorID contractorID,
        const SerializedEquivalent equivalent,
        const TransactionUUID &transactionUUID,
        const Message::SerializedType messageType,
        BytesShared message,
        size_t messageBytesCount) override;
    vector<tuple<ContractorID, BytesShared, Message::SerializedType>> allMessages() override;
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
    const string logHeader() const;
    sqlite3 *mDataBase = nullptr;
    string mTableName;
    Logger &mLog;
};
#endif //VTCPD_COMMUNICATORMESSAGESQUEUEHANDLERSQLITE_H
