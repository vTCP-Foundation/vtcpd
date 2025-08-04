#ifndef VTCPD_INTERFACES_COMMUNICATORMESSAGESQUEUEHANDLER_H
#define VTCPD_INTERFACES_COMMUNICATORMESSAGESQUEUEHANDLER_H

#include "../../../logger/Logger.h"
#include "../../../common/Types.h"
#include "../../../network/messages/Message.hpp"
#include "../../../transactions/transactions/base/TransactionUUID.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/memory/MemoryUtils.h"

#include <tuple>
#include <vector>
#include <memory>

using namespace std;

class CommunicatorMessagesQueueHandler
{
public:
    virtual ~CommunicatorMessagesQueueHandler() = default;

    virtual void saveRecord(
        ContractorID contractorID,
        const SerializedEquivalent equivalent,
        const TransactionUUID &transactionUUID,
        const Message::SerializedType messageType,
        BytesShared message,
        size_t messageBytesCount) = 0;

    virtual vector<tuple<ContractorID, BytesShared, Message::SerializedType>> allMessages() = 0;

    virtual void deleteRecord(
        ContractorID contractorID,
        const SerializedEquivalent equivalent,
        const Message::SerializedType messageType) = 0;

    virtual void deleteRecord(
        ContractorID contractorID,
        const TransactionUUID &transactionUUID) = 0;
};

#endif //VTCPD_INTERFACES_COMMUNICATORMESSAGESQUEUEHANDLER_H 