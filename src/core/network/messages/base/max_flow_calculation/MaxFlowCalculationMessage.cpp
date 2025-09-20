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
}

vector<BaseAddress::Shared> MaxFlowCalculationMessage::targetAddresses() const
{
    return mTargetAddresses;
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

    return serializer.collect();
}

const size_t MaxFlowCalculationMessage::kOffsetToInheritedBytes() const
{
    auto kOffset = SenderMessage::kOffsetToInheritedBytes() + BytesSerializer::kSerializedByteSize;
    for (const auto &address : mTargetAddresses) {
        kOffset += address->serializedSize();
    }
    return kOffset;
}
