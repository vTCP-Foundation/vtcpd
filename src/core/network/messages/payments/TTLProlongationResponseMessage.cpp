#include "TTLProlongationResponseMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"
#include "../../../common/serialization/BytesSerializer.h"

TTLProlongationResponseMessage::TTLProlongationResponseMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID& transactionUUID,
    const OperationState state) :

    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID),
    mState(state)
{}

TTLProlongationResponseMessage::TTLProlongationResponseMessage(
    BytesShared buffer):

    TransactionMessage(buffer)
{
    size_t currentOffset = TransactionMessage::kOffsetToInheritedBytes();
    //----------------------------------------------------
    BytesDeserializer deserializer(buffer, currentOffset);
    SerializedOperationState state;
    deserializer.copyInto(&state);
    mState = (OperationState) state;
}

const TTLProlongationResponseMessage::OperationState TTLProlongationResponseMessage::state() const
{
    return mState;
}

pair<BytesShared, size_t> TTLProlongationResponseMessage::serializeToBytes() const
{
    BytesSerializer serializer;
    serializer.enqueue(TransactionMessage::serializeToBytes());
    serializer.copy((SerializedOperationState)mState);
    return serializer.collect();
}

const Message::MessageType TTLProlongationResponseMessage::typeID() const
{
    return Message::Payments_TTLProlongationResponse;
}
