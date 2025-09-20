#include "InitiateMaxFlowCalculationMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"

InitiateMaxFlowCalculationMessage::InitiateMaxFlowCalculationMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared>& senderAddresses,
    bool isSenderGateway,
    uint8_t hopsCount):
    SenderMessage(
        equivalent,
        senderAddresses),
    mIsSenderGateway(isSenderGateway),
    mHopsCount(hopsCount)
{}

InitiateMaxFlowCalculationMessage::InitiateMaxFlowCalculationMessage(
    BytesShared buffer) : SenderMessage(buffer)
{
    size_t bytesBufferOffset = SenderMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, bytesBufferOffset);

    deserializer.copyInto(&mIsSenderGateway);
    deserializer.copyInto(&mHopsCount);
}

bool InitiateMaxFlowCalculationMessage::isSenderGateway() const
{
    return mIsSenderGateway;
}

uint8_t InitiateMaxFlowCalculationMessage::getHopsCount() const
{
    return mHopsCount;
}

const Message::MessageType InitiateMaxFlowCalculationMessage::typeID() const
{
    return Message::MaxFlow_InitiateCalculation;
}

pair<BytesShared, size_t> InitiateMaxFlowCalculationMessage::serializeToBytes() const
{
    auto parentBytesAndCount = SenderMessage::serializeToBytes();
    size_t bytesCount =
        parentBytesAndCount.second +
        sizeof(mIsSenderGateway) +
        sizeof(mHopsCount);

    // Use BytesSerializer for consistent serialization
    BytesSerializer serializer;
    serializer.enqueue(parentBytesAndCount);
    serializer.copy(mIsSenderGateway);
    serializer.copy(mHopsCount);

    return serializer.collect();
}
