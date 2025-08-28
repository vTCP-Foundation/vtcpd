#include "MaxFlowCalculationMessage.h"

MaxFlowCalculationMessage::MaxFlowCalculationMessage(
    const SerializedEquivalent equivalent,
    ContractorID idOnReceiverSide,
    vector<BaseAddress::Shared> targetAddresses) :

    SenderMessage(
        equivalent,
        idOnReceiverSide),

    mTargetAddresses(targetAddresses)
{
}

MaxFlowCalculationMessage::MaxFlowCalculationMessage(
    const SerializedEquivalent equivalent,
    ContractorID idOnReceiverSide,
    vector<BaseAddress::Shared> targetAddresses,
    vector<SerializedEquivalent> exchangeEquivalents) :

    SenderMessage(
        equivalent,
        idOnReceiverSide),

    mTargetAddresses(targetAddresses),
    mExchangeEquivalents(exchangeEquivalents)
{
}

MaxFlowCalculationMessage::MaxFlowCalculationMessage(
    BytesShared buffer) : SenderMessage(buffer)
{
    auto bytesBufferOffset = SenderMessage::kOffsetToInheritedBytes();

    byte_t senderAddressesCnt;
    memcpy(
        &senderAddressesCnt,
        buffer.get() + bytesBufferOffset,
        sizeof(byte_t));
    bytesBufferOffset += sizeof(byte_t);

    for (int idx = 0; idx < senderAddressesCnt; idx++) {
        auto targetAddress = deserializeAddress(
                                 buffer.get() + bytesBufferOffset);
        mTargetAddresses.push_back(targetAddress);
        bytesBufferOffset += targetAddress->serializedSize();
    }

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

vector<BaseAddress::Shared> MaxFlowCalculationMessage::targetAddresses() const
{
    return mTargetAddresses;
}

vector<SerializedEquivalent> MaxFlowCalculationMessage::exchangeEquivalents() const
{
    return mExchangeEquivalents;
}

pair<BytesShared, size_t> MaxFlowCalculationMessage::serializeToBytes() const
{
    auto parentBytesAndCount = SenderMessage::serializeToBytes();
    size_t bytesCount = parentBytesAndCount.second + sizeof(byte_t) + sizeof(uint8_t);
    for (const auto &address : mTargetAddresses) {
        bytesCount += address->serializedSize();
    }
    bytesCount += mExchangeEquivalents.size() * sizeof(SerializedEquivalent);

    BytesShared dataBytesShared = tryCalloc(bytesCount);
    size_t dataBytesOffset = 0;

    memcpy(
        dataBytesShared.get(),
        parentBytesAndCount.first.get(),
        parentBytesAndCount.second);
    dataBytesOffset += parentBytesAndCount.second;

    auto targetAddressesCnt = (byte_t)mTargetAddresses.size();
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &targetAddressesCnt,
        sizeof(byte_t));
    dataBytesOffset += sizeof(byte_t);

    for (auto &targetAddress : mTargetAddresses) {
        auto serializedData = targetAddress->serializeToBytes();
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            serializedData.get(),
            targetAddress->serializedSize());
        dataBytesOffset += targetAddress->serializedSize();
    }

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

    return make_pair(
               dataBytesShared,
               bytesCount);
}

const size_t MaxFlowCalculationMessage::kOffsetToInheritedBytes() const
{
    auto kOffset = SenderMessage::kOffsetToInheritedBytes() + sizeof(byte_t);
    for (const auto &address : mTargetAddresses) {
        kOffset += address->serializedSize();
    }
    kOffset += sizeof(uint8_t); // count of exchange equivalents
    kOffset += mExchangeEquivalents.size() * sizeof(SerializedEquivalent);
    return kOffset;
}
