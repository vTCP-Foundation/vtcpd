#include "SenderMessage.h"

SenderMessage::SenderMessage(
    const SerializedEquivalent equivalent,
    ContractorID idOnReceiverSide) : EquivalentMessage(equivalent),
    idOnReceiverSide(idOnReceiverSide),
    senderAddresses({})
{
}

SenderMessage::SenderMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses) : EquivalentMessage(equivalent),
    idOnReceiverSide(0),
    senderAddresses(senderAddresses)
{
}

SenderMessage::SenderMessage(
    BytesShared buffer) : EquivalentMessage(buffer)
{
    auto bytesBufferOffset = EquivalentMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, bytesBufferOffset);

    // Use BytesDeserializer for consistent offset management
    deserializer.copyInto(&idOnReceiverSide);

    byte_t senderAddressesCnt;
    deserializer.copyInto(&senderAddressesCnt);

    for (int idx = 0; idx < senderAddressesCnt; idx++) {
        // Get current position from deserializer for address parsing
        auto currentOffset = deserializer.getCurrentOffset();
        auto senderAddress = deserializeAddress(buffer.get() + currentOffset);
        senderAddresses.push_back(senderAddress);

        // Keep deserializer in sync by advancing past the address data
        deserializer.skipBytes(senderAddress->serializedSize());
    }
}

pair<BytesShared, size_t> SenderMessage::serializeToBytes() const
{
    BytesSerializer serializer;

    serializer.enqueue(EquivalentMessage::serializeToBytes());
    serializer.copy(idOnReceiverSide);
    serializer.copy((byte_t)senderAddresses.size());
    for (const auto &address : senderAddresses) {
        serializer.enqueue(
            address->serializeToBytes(),
            address->serializedSize());
    }
    return serializer.collect();
}

const size_t SenderMessage::kOffsetToInheritedBytes() const
{
    auto kOffset =
        EquivalentMessage::kOffsetToInheritedBytes() +
        BytesSerializer::kSerializedContractorIDSize +
        BytesSerializer::kSerializedByteSize;
    for (const auto &address : senderAddresses) {
        kOffset += address->serializedSize();
    }
    return kOffset;
}
