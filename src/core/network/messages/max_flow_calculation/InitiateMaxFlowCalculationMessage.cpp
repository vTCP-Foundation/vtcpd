#include "InitiateMaxFlowCalculationMessage.h"

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

    memcpy(
        &mIsSenderGateway,
        buffer.get() + bytesBufferOffset,
        sizeof(byte));
    bytesBufferOffset += sizeof(byte);

    memcpy(
        &mHopsCount,
        buffer.get() + bytesBufferOffset,
        sizeof(uint8_t));
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

    BytesShared dataBytesShared = tryCalloc(bytesCount);
    size_t dataBytesOffset = 0;

    // Marshal parent message bytes
    memcpy(
        dataBytesShared.get(),
        parentBytesAndCount.first.get(),
        parentBytesAndCount.second);
    dataBytesOffset += parentBytesAndCount.second;

    // Marshal mIsSenderGateway
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mIsSenderGateway,
        sizeof(mIsSenderGateway));
    dataBytesOffset += sizeof(mIsSenderGateway);

    // Marshal mHopsCount
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mHopsCount,
        sizeof(mHopsCount));

    return make_pair(dataBytesShared, bytesCount);
}
