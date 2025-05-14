#include "InitiateMaxFlowCalculationMessage.h"

InitiateMaxFlowCalculationMessage::InitiateMaxFlowCalculationMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    bool isSenderGateway,
    uint8_t hopsCnt):
    SenderMessage(
        equivalent,
        senderAddresses),
    mIsSenderGateway(isSenderGateway),
    mHopsCnt(hopsCnt)
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
        &mHopsCnt,
        buffer.get() + bytesBufferOffset,
        sizeof(uint8_t));
}

bool InitiateMaxFlowCalculationMessage::isSenderGateway() const
{
    return mIsSenderGateway;
}

HopsCount_t InitiateMaxFlowCalculationMessage::HopsCount() const
{
    return mHopsCnt;
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
        sizeof(byte);

    BytesShared dataBytesShared = tryCalloc(bytesCount);
    size_t dataBytesOffset = 0;
    //----------------------------------------------------
    memcpy(
        dataBytesShared.get(),
        parentBytesAndCount.first.get(),
        parentBytesAndCount.second);
    dataBytesOffset += parentBytesAndCount.second;
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mIsSenderGateway,
        sizeof(byte));
    dataBytesOffset += sizeof(byte);
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mHopsCnt,
        sizeof(uint8_t));

    return make_pair(
               dataBytesShared,
               bytesCount);
}
