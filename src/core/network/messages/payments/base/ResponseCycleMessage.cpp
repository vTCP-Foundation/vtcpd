#include "ResponseCycleMessage.h"
#include "../../../../common/serialization/BytesDeserializer.h"
#include "../../../../common/serialization/BytesSerializer.h"

ResponseCycleMessage::ResponseCycleMessage(
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

ResponseCycleMessage::ResponseCycleMessage(
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

const ResponseCycleMessage::OperationState ResponseCycleMessage::state() const
{
    return mState;
}

const size_t ResponseCycleMessage::kOffsetToInheritedBytes() const
{
    return TransactionMessage::kOffsetToInheritedBytes()
           + BytesSerializer::kSerializedByteSize;
}

pair<BytesShared, size_t> ResponseCycleMessage::serializeToBytes() const
{
    // TODO: Serialization architecture optimization needed across entire inheritance chain:
    // This base class pattern causes redundant parent serialization in all child classes.
    // Each child calls parent::serializeToBytes() then copies that buffer again via enqueue().
    // Consider: void serializeToSerializer(BytesSerializer& serializer) pattern to avoid copies.
    BytesSerializer serializer;
    serializer.enqueue(TransactionMessage::serializeToBytes());
    serializer.copy((SerializedOperationState)mState);
    return serializer.collect();
}
