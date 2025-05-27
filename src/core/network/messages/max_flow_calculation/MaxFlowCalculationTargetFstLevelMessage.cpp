#include "MaxFlowCalculationTargetFstLevelMessage.h"

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

    memcpy(
        &mIsTargetGateway,
        buffer.get() + bytesBufferOffset,
        sizeof(byte));

    bytesBufferOffset += sizeof(byte);

    memcpy(
        &mHopsCnt,
        buffer.get() + bytesBufferOffset,
        sizeof(byte));
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
    memcpy(
        dataBytesShared.get(),
        parentBytesAndCount.first.get(),
        parentBytesAndCount.second);
    dataBytesOffset += parentBytesAndCount.second;

    // Marshal mIsTargetGateway
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mIsTargetGateway,
        sizeof(mIsTargetGateway));

    dataBytesOffset += sizeof(mIsTargetGateway);

    // Marshal mHopsCnt
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mHopsCnt,
        sizeof(mHopsCnt));

    return make_pair(dataBytesShared, bytesCount);
}

const uint8_t MaxFlowCalculationTargetFstLevelMessage::getHopsCount() const
{
    return this->mHopsCnt;
}
