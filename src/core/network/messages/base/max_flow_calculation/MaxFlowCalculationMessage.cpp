#include "MaxFlowCalculationMessage.h"
#include "../../../../common/serialization/BytesDeserializer.h"
#include "../../../../common/serialization/BytesSerializer.h"

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
    auto deserializer = BytesDeserializer(
        buffer,
        SenderMessage::kOffsetToInheritedBytes());

    byte_t senderAddressesCnt;
    deserializer.copyInto(&senderAddressesCnt);

    size_t currentOffset = SenderMessage::kOffsetToInheritedBytes() + BytesSerializer::kSerializedByteSize;
    for (int idx = 0; idx < senderAddressesCnt; idx++) {
        auto targetAddress = deserializeAddress(
                                 buffer.get() + currentOffset);
        mTargetAddresses.push_back(targetAddress);
        currentOffset += targetAddress->serializedSize();
    }

    // Deserialize exchange equivalents count
    BytesDeserializer exchangeEquivalentsCntDeserializer(
        buffer,
        currentOffset);
    uint8_t exchangeEquivalentsCnt;
    exchangeEquivalentsCntDeserializer.copyInto(&exchangeEquivalentsCnt);
    currentOffset += BytesSerializer::kSerializedByteSize;

    // Deserialize each exchange equivalent
    mExchangeEquivalents.reserve(exchangeEquivalentsCnt);
    for (uint8_t idx = 0; idx < exchangeEquivalentsCnt; idx++) {
        BytesDeserializer equivalentDeserializer(
            buffer,
            currentOffset);
        SerializedEquivalent equivalent;
        equivalentDeserializer.copyInto(&equivalent);
        mExchangeEquivalents.push_back(equivalent);
        currentOffset += BytesSerializer::kSerializedEquivalentSize;
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
    // TODO: Serialization architecture optimization needed across entire inheritance chain:
    // This base class pattern causes redundant parent serialization in all child classes.
    // Each child calls parent::serializeToBytes() then copies that buffer again via enqueue().
    // Consider: void serializeToSerializer(BytesSerializer& serializer) pattern to avoid copies.
    auto serializer = BytesSerializer();

    // Serialize parent data
    serializer.enqueue(SenderMessage::serializeToBytes());

    // Serialize target addresses count
    auto targetAddressesCnt = (byte_t)mTargetAddresses.size();
    serializer.copy(targetAddressesCnt);

    // Serialize target addresses
    for (auto &targetAddress : mTargetAddresses) {
        serializer.enqueue(targetAddress->serializeToBytes(), targetAddress->serializedSize());
    }
    // Serialize exchange equivalents count
    auto exchangeEquivalentsCnt = (uint8_t)mExchangeEquivalents.size();
    serializer.copy(exchangeEquivalentsCnt);

    // Serialize each exchange equivalent
    for (const auto &equivalent : mExchangeEquivalents) {
        serializer.copy(equivalent);
    }

    return serializer.collect();
}

const size_t MaxFlowCalculationMessage::kOffsetToInheritedBytes() const
{
    auto kOffset = SenderMessage::kOffsetToInheritedBytes() + BytesSerializer::kSerializedByteSize;
    for (const auto &address : mTargetAddresses) {
        kOffset += address->serializedSize();
    }
    kOffset += BytesSerializer::kSerializedByteSize; // count of exchange equivalents
    kOffset += mExchangeEquivalents.size() * BytesSerializer::kSerializedEquivalentSize;
    return kOffset;
}
