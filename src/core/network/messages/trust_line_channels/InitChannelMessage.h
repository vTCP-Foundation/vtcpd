#ifndef VTCPD_INITCHANNELMESSAGE_H
#define VTCPD_INITCHANNELMESSAGE_H

#include "../base/transaction/TransactionMessage.h"
#include "../../../contractors/Contractor.h"
#include "../../../crypto/sphincskeys.h"

namespace sphincs = crypto::sphincs;

class InitChannelMessage : public TransactionMessage
{

public:
    typedef shared_ptr<InitChannelMessage> Shared;

public:
    InitChannelMessage(
        vector<BaseAddress::Shared> senderAddresses,
        const TransactionUUID &transactionUUID,
        Contractor::Shared contractor,
        const sphincs::PublicKey::Shared paymentPublicKey);

    InitChannelMessage(
        BytesShared buffer);

    const MessageType typeID() const override;

    const ContractorID contractorID() const;

    const MsgEncryptor::PublicKey::Shared publicKey() const;

    // Returns sender payment public key shared during channel init.
    const sphincs::PublicKey::Shared paymentPublicKey() const;

    pair<BytesShared, size_t> serializeToBytes() const override;

protected:
    ContractorID mContractorID;
    MsgEncryptor::PublicKey::Shared mPublicKey;
    // Sender payment public key used for receipt verification.
    sphincs::PublicKey::Shared mPaymentPublicKey;
};


#endif //VTCPD_INITCHANNELMESSAGE_H
