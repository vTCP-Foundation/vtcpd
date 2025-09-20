#include "MaxFlowCalculationTargetFstLevelMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"

MaxFlowCalculationTargetFstLevelMessage::MaxFlowCalculationTargetFstLevelMessage(
    const SerializedEquivalent equivalent,
    ContractorID idOnReceiverSide,
    vector<BaseAddress::Shared> targetAddresses,
    bool isTargetGateway,
    HopsCount_t hopsCount):
    MaxFlowCalculationMessage(
        equivalent,
        idOnReceiverSide,
        targetAddresses),
    mIsTargetGateway(isTargetGateway),
    mHopsCnt(hopsCount)
{}

MaxFlowCalculationTargetFstLevelMessage::MaxFlowCalculationTargetFstLevelMessage(
    BytesShared buffer) : MaxFlowCalculationMessage(buffer)
{
    size_t bytesBufferOffset = MaxFlowCalculationMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, bytesBufferOffset);

    deserializer.copyInto(&mIsTargetGateway);
    deserializer.copyInto(&mHopsCnt);
}

bool MaxFlowCalculationTargetFstLevelMessage::isTargetGateway() const
{
    return mIsTargetGateway;
}

const Message::MessageType MaxFlowCalculationTargetFstLevelMessage::typeID() const
{
    return Message::MessageType::MaxFlow_CalculationTargetFirstLevel;
}

pair<BytesShared, size_t> MaxFlowCalculationTargetFstLevelMessage::serializeToBytes() const
{
    auto parentBytesAndCount = MaxFlowCalculationMessage::serializeToBytes();
    size_t bytesCount =
        parentBytesAndCount.second +
        sizeof(mIsTargetGateway) +
        sizeof(mHopsCnt);

    BytesShared dataBytesShared = tryCalloc(bytesCount);
    size_t dataBytesOffset = 0;

    // Marshal parent message bytes
    // Use BytesSerializer for consistent serialization
    BytesSerializer serializer;
    serializer.enqueue(parentBytesAndCount);
    serializer.copy(mIsTargetGateway);
    serializer.copy(mHopsCnt);

    return serializer.collect();
}

const uint8_t MaxFlowCalculationTargetFstLevelMessage::getHopsCount() const
{
    return this->mHopsCnt;
}
