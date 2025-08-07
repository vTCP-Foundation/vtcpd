#ifndef VTCPD_PUBLICKEYSSHARINGINITMESSAGE_H
#define VTCPD_PUBLICKEYSSHARINGINITMESSAGE_H

#include "PublicKeyMessage.h"

class PublicKeysSharingInitMessage : public PublicKeyMessage
{

public:
    typedef shared_ptr<PublicKeysSharingInitMessage> Shared;

public:
    PublicKeysSharingInitMessage(
        const SerializedEquivalent equivalent,
        Contractor::Shared contractor,
        const TransactionUUID &transactionUUID,
        const KeysCount keysCount,
        const crypto::sphincs::PublicKey::Shared publicKey);

    PublicKeysSharingInitMessage(
        BytesShared buffer);

    const KeysCount keysCount() const;

    const MessageType typeID() const override;

    virtual pair<BytesShared, size_t> serializeToBytes() const override;

private:
    KeysCount mKeysCount;
};


#endif //VTCPD_PUBLICKEYSSHARINGINITMESSAGE_H
