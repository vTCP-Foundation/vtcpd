#ifndef VTCPD_CONFIRMCHANNELMESSAGE_H
#define VTCPD_CONFIRMCHANNELMESSAGE_H

#include "../base/transaction/TransactionMessage.h"
#include "../../../contractors/Contractor.h"

class ConfirmChannelMessage : public TransactionMessage
{

public:
    typedef shared_ptr<ConfirmChannelMessage> Shared;

public:
    ConfirmChannelMessage(
        const TransactionUUID &transactionUUID,
        Contractor::Shared contractor);

    ConfirmChannelMessage(
        BytesShared buffer);

    const MessageType typeID() const override;
};


#endif //VTCPD_CONFIRMCHANNELMESSAGE_H
