#ifndef VTCPD_PARTICIPANTVOTEMESSAGE_H
#define VTCPD_PARTICIPANTVOTEMESSAGE_H

#include "../base/transaction/TransactionMessage.h"
#include "../../../crypto/sphincsscheme.h"

using namespace crypto;

class ParticipantVoteMessage : public TransactionMessage
{

public:
    typedef shared_ptr<ParticipantVoteMessage> Shared;

    enum OperationState
    {
        Accepted = 1,
        Rejected = 2,
    };

public:
    ParticipantVoteMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> &senderAddresses,
        const TransactionUUID &transactionUUID,
        sphincs::Signature::Shared signature = nullptr);

    ParticipantVoteMessage(
        BytesShared buffer);

    const MessageType typeID() const override;

    const OperationState state() const;

    const sphincs::Signature::Shared signature() const;

    pair<BytesShared, size_t> serializeToBytes() const override;

private:
    typedef byte_t SerializedOperationState;

private:
    OperationState mState;
    sphincs::Signature::Shared mSignature;
};

#endif // VTCPD_PARTICIPANTVOTEMESSAGE_H
