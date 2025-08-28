#include "InitiateMaxFlowForExchangeCalculationMessage.h"

InitiateMaxFlowForExchangeCalculationMessage::InitiateMaxFlowForExchangeCalculationMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared>& senderAddresses,
    bool isSenderGateway,
    uint8_t hopsCount,
    vector<SerializedEquivalent> exchangeEquivalents):

    SenderMessage(
        equivalent,
        senderAddresses),
    mIsSenderGateway(isSenderGateway),
    mHopsCount(hopsCount),
    mExchangeEquivalents(exchangeEquivalents)
{
}

InitiateMaxFlowForExchangeCalculationMessage::InitiateMaxFlowForExchangeCalculationMessage(
    BytesShared buffer) : SenderMessage(buffer)
{
    size_t bytesBufferOffset = SenderMessage::kOffsetToInheritedBytes();

    memcpy(
        &mIsSenderGateway,
        buffer.get() + bytesBufferOffset,
        sizeof(byte_t));
    bytesBufferOffset += sizeof(byte_t);

    memcpy(
        &mHopsCount,
        buffer.get() + bytesBufferOffset,
        sizeof(uint8_t));
    bytesBufferOffset += sizeof(uint8_t);

    uint8_t exchangeEquivalentsCnt;
    memcpy(
        &exchangeEquivalentsCnt,
        buffer.get() + bytesBufferOffset,
        sizeof(uint8_t));
    bytesBufferOffset += sizeof(uint8_t);

    for (uint8_t idx = 0; idx < exchangeEquivalentsCnt; idx++) {
        SerializedEquivalent equivalent;
        memcpy(
            &equivalent,
            buffer.get() + bytesBufferOffset,
            sizeof(SerializedEquivalent));
        mExchangeEquivalents.push_back(equivalent);
        bytesBufferOffset += sizeof(SerializedEquivalent);
    }
}

bool InitiateMaxFlowForExchangeCalculationMessage::isSenderGateway() const
{
    return mIsSenderGateway;
}

uint8_t InitiateMaxFlowForExchangeCalculationMessage::getHopsCount() const
{
    return mHopsCount;
}

vector<SerializedEquivalent> InitiateMaxFlowForExchangeCalculationMessage::exchangeEquivalents() const
{
    return mExchangeEquivalents;
}

const Message::MessageType InitiateMaxFlowForExchangeCalculationMessage::typeID() const
{
    return Message::MaxFlow_InitiateExchangeCalculation;
}

pair<BytesShared, size_t> InitiateMaxFlowForExchangeCalculationMessage::serializeToBytes() const
{
    auto parentBytesAndCount = SenderMessage::serializeToBytes();
    size_t bytesCount =
        parentBytesAndCount.second +
        sizeof(mIsSenderGateway) +
        sizeof(mHopsCount) +
        sizeof(uint8_t) + // count of exchange equivalents
        mExchangeEquivalents.size() * sizeof(SerializedEquivalent);

    BytesShared dataBytesShared = tryCalloc(bytesCount);
    size_t dataBytesOffset = 0;

    memcpy(
        dataBytesShared.get(),
        parentBytesAndCount.first.get(),
        parentBytesAndCount.second);
    dataBytesOffset += parentBytesAndCount.second;

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mIsSenderGateway,
        sizeof(mIsSenderGateway));
    dataBytesOffset += sizeof(mIsSenderGateway);

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mHopsCount,
        sizeof(mHopsCount));
    dataBytesOffset += sizeof(mHopsCount);

    auto exchangeEquivalentsCnt = (uint8_t)mExchangeEquivalents.size();
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &exchangeEquivalentsCnt,
        sizeof(uint8_t));
    dataBytesOffset += sizeof(uint8_t);

    for (const auto &equivalent : mExchangeEquivalents) {
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            &equivalent,
            sizeof(SerializedEquivalent));
        dataBytesOffset += sizeof(SerializedEquivalent);
    }

    return make_pair(dataBytesShared, bytesCount);
}

const size_t InitiateMaxFlowForExchangeCalculationMessage::kOffsetToInheritedBytes() const
{
    return SenderMessage::kOffsetToInheritedBytes() +
           sizeof(mIsSenderGateway) +
           sizeof(mHopsCount) +
           sizeof(uint8_t) + // count of exchange equivalents
           mExchangeEquivalents.size() * sizeof(SerializedEquivalent);
}