#ifndef VTCPD_PARTICIPANTSPUBLICKEYSMESSAGE_H
#define VTCPD_PARTICIPANTSPUBLICKEYSMESSAGE_H

#include "../base/transaction/TransactionMessage.h"
#include "../../../crypto/sphincsscheme.h"

#include <map>

using namespace crypto;

class ParticipantsPublicKeysMessage : public TransactionMessage
{

public:
    typedef shared_ptr<ParticipantsPublicKeysMessage> Shared;

public:
    ParticipantsPublicKeysMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> &senderAddresses,
        const TransactionUUID &transactionUUID,
        const map<PaymentNodeID, sphincs::PublicKey::Shared>& publicKeys);

    ParticipantsPublicKeysMessage(
        BytesShared buffer);

    const MessageType typeID() const override;

    pair<BytesShared, size_t> serializeToBytes() const override;

    const map<PaymentNodeID, sphincs::PublicKey::Shared>& publicKeys() const;

private:
    map<PaymentNodeID, sphincs::PublicKey::Shared> mPublicKeys;
};


#endif //VTCPD_PARTICIPANTSPUBLICKEYSMESSAGE_H
