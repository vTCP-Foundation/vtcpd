#include "ResponseMessage.h"
#include "../../../../common/serialization/BytesDeserializer.h"
#include "../../../../common/serialization/BytesSerializer.h"


ResponseMessage::ResponseMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID& transactionUUID,
    const PathID &pathID,
    const OperationState state) :

    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID),
    mPathID(pathID),
    mState(state)
{}

ResponseMessage::ResponseMessage(
    BytesShared buffer):

    TransactionMessage(buffer)
{
    size_t currentOffset = TransactionMessage::kOffsetToInheritedBytes();
    //----------------------------------------------------
    BytesDeserializer deserializer(buffer, currentOffset);
    deserializer.copyInto(&mPathID);
    //----------------------------------------------------
    SerializedOperationState state;
    deserializer.copyInto(&state);
    mState = (OperationState) state;
}

const ResponseMessage::OperationState ResponseMessage::state() const
{
    return mState;
}

const PathID ResponseMessage::pathID() const
{
    return mPathID;
}

const size_t ResponseMessage::kOffsetToInheritedBytes() const
{
    return TransactionMessage::kOffsetToInheritedBytes()
           + BytesSerializer::kSerializedPathIDSize
           + BytesSerializer::kSerializedByteSize;
}

pair<BytesShared, size_t> ResponseMessage::serializeToBytes() const
{
    // TODO: Serialization architecture optimization needed across entire inheritance chain:
    // This base class pattern causes redundant parent serialization in all child classes.
    // Each child calls parent::serializeToBytes() then copies that buffer again via enqueue().
    // Consider: void serializeToSerializer(BytesSerializer& serializer) pattern to avoid copies.
    BytesSerializer serializer;
    serializer.enqueue(TransactionMessage::serializeToBytes());
    serializer.copy(mPathID);
    serializer.copy((SerializedOperationState)mState);
    return serializer.collect();
}

