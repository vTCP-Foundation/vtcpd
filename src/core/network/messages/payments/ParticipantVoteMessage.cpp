#include "ParticipantVoteMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"
#include "../../../common/serialization/BytesSerializer.h"

ParticipantVoteMessage::ParticipantVoteMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    sphincs::Signature::Shared signature) :
    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID),
    mSignature(signature)
{}

ParticipantVoteMessage::ParticipantVoteMessage(
    BytesShared buffer):
    TransactionMessage(buffer)
{
    auto currentOffset = TransactionMessage::kOffsetToInheritedBytes();

    BytesDeserializer deserializer(buffer, currentOffset);
    SerializedOperationState state;
    deserializer.copyInto(&state);
    mState = (OperationState) state;
    if (mState == Accepted) {
        auto signature = make_shared<sphincs::Signature>(
                             buffer.get() + currentOffset + sizeof(SerializedOperationState));
        mSignature = signature;
    }
}

const Message::MessageType ParticipantVoteMessage::typeID() const
{
    return Message::Payments_ParticipantVote;
}

const ParticipantVoteMessage::OperationState ParticipantVoteMessage::state() const
{
    return mState;
}

const sphincs::Signature::Shared ParticipantVoteMessage::signature() const
{
    return mSignature;
}

pair<BytesShared, size_t> ParticipantVoteMessage::serializeToBytes() const
{
    BytesSerializer serializer;
    serializer.enqueue(TransactionMessage::serializeToBytes());

    if (mSignature != nullptr) {
        serializer.copy((SerializedOperationState)ParticipantVoteMessage::Accepted);
        serializer.copy(
            mSignature->data(),
            sphincs::Signature::signatureSize());
    } else {
        serializer.copy((SerializedOperationState)ParticipantVoteMessage::Rejected);
    }

    return serializer.collect();
}