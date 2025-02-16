#ifndef VTCPD_OBSERVINGTRANSACTIONSREQUESTMESSAGE_H
#define VTCPD_OBSERVINGTRANSACTIONSREQUESTMESSAGE_H

#include "base/ObservingMessage.hpp"
#include "../../transactions/transactions/base/TransactionUUID.h"
#include <vector>

class ObservingTransactionsRequestMessage : public ObservingMessage
{

public:
    typedef shared_ptr<ObservingTransactionsRequestMessage> Shared;

public:
    ObservingTransactionsRequestMessage(
        vector<pair<TransactionUUID, BlockNumber>> transactions);

    const MessageType typeID() const override;

    BytesShared serializeToBytes() const override;

    size_t serializedSize() const override;

private:
    vector<pair<TransactionUUID, BlockNumber>> mTransactions;
};


#endif //VTCPD_OBSERVINGTRANSACTIONSREQUESTMESSAGE_H
