#include "UpdateChannelAddressesMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"

UpdateChannelAddressesMessage::UpdateChannelAddressesMessage(
    Contractor::Shared contractor,
    const TransactionUUID &transactionUUID,
    vector<BaseAddress::Shared> newSenderAddresses) : TransactionMessage(0,
                contractor->ownIdOnContractorSide(),
                transactionUUID),
    mNewSenderAddresses(newSenderAddresses)
{
    encrypt(contractor);
}

UpdateChannelAddressesMessage::UpdateChannelAddressesMessage(
    BytesShared buffer) : TransactionMessage(buffer)
{
    size_t bytesBufferOffset = TransactionMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, bytesBufferOffset);

    byte_t senderAddressesCnt;
    deserializer.copyInto(&senderAddressesCnt);
    mNewSenderAddresses.reserve(senderAddressesCnt);

    for (int idx = 0; idx < senderAddressesCnt; idx++) {
        // Get current position from deserializer for address parsing
        auto currentOffset = deserializer.getCurrentOffset();
        auto senderAddress = deserializeAddress(buffer.get() + currentOffset);
        mNewSenderAddresses.push_back(senderAddress);

        // Keep deserializer in sync by advancing past the address data
        deserializer.skipBytes(senderAddress->serializedSize());
    }
}

const Message::MessageType UpdateChannelAddressesMessage::typeID() const
{
    return Message::Channel_UpdateAddresses;
}

vector<BaseAddress::Shared> UpdateChannelAddressesMessage::newSenderAddresses() const
{
    return mNewSenderAddresses;
}

pair<BytesShared, size_t> UpdateChannelAddressesMessage::serializeToBytes() const
{
    auto parentBytesAndCount = TransactionMessage::serializeToBytes();

    size_t bytesCount = parentBytesAndCount.second + sizeof(byte_t);
    for (const auto &address : mNewSenderAddresses) {
        bytesCount += address->serializedSize();
    }

    // Use BytesSerializer for consistent serialization
    BytesSerializer serializer;
    serializer.enqueue(parentBytesAndCount);
    serializer.copy((byte_t)mNewSenderAddresses.size());

    for (const auto &address : mNewSenderAddresses) {
        auto serializedAddress = address->serializeToBytes();
        serializer.copy(serializedAddress.get(), address->serializedSize());
    }

    return serializer.collect();
}