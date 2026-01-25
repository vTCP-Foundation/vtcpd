#ifndef VTCPD_CONFIRMCHANNELMESSAGE_H
#define VTCPD_CONFIRMCHANNELMESSAGE_H

#include "../base/transaction/TransactionMessage.h"
#include "../../../contractors/Contractor.h"
#include "../../../crypto/sphincskeys.h"

namespace sphincs = crypto::sphincs;

class ConfirmChannelMessage : public TransactionMessage
{

public:
    typedef shared_ptr<ConfirmChannelMessage> Shared;

public:
    ConfirmChannelMessage(
        const TransactionUUID &transactionUUID,
        Contractor::Shared contractor,
        const sphincs::PublicKey::Shared paymentPublicKey);

    ConfirmChannelMessage(
        BytesShared buffer);

    const MessageType typeID() const override;

    // Returns sender payment public key shared during channel confirmation.
    const sphincs::PublicKey::Shared paymentPublicKey() const;

    // Serializes confirmation payload with sender payment public key.
    pair<BytesShared, size_t> serializeToBytes() const override;

protected:
    // Sender payment public key used for receipt verification.
    sphincs::PublicKey::Shared mPaymentPublicKey;
};


#endif //VTCPD_CONFIRMCHANNELMESSAGE_H
