#include "PublicKeysSharingInitMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"

PublicKeysSharingInitMessage::PublicKeysSharingInitMessage(
    const SerializedEquivalent equivalent,
    Contractor::Shared contractor,
    const TransactionUUID &transactionUUID,
    const KeysCount keysCount,
    const KeyNumber number,
    const lamport::PublicKey::Shared publicKey):
    PublicKeyMessage(
        equivalent,
        contractor,
        transactionUUID,
        number,
        publicKey),
    mKeysCount(keysCount)
{}

PublicKeysSharingInitMessage::PublicKeysSharingInitMessage(
    BytesShared buffer) :
    PublicKeyMessage(buffer)
{
    auto bytesBufferOffset = PublicKeyMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, bytesBufferOffset);

    deserializer.copyInto(&mKeysCount);
}

const Message::MessageType PublicKeysSharingInitMessage::typeID() const
{
    return Message::TrustLines_PublicKeysSharingInit;
}

const KeyNumber PublicKeysSharingInitMessage::keysCount() const
{
    return mKeysCount;
}

pair<BytesShared, size_t> PublicKeysSharingInitMessage::serializeToBytes() const
{
    // TODO: Performance optimization - parent data is serialized twice:
    // 1. PublicKeyMessage::serializeToBytes() creates buffer
    // 2. serializer.enqueue() copies that buffer again
    // Consider passing serializer down inheritance chain to avoid redundant allocation/copy
    const auto parentBytesAndCount = PublicKeyMessage::serializeToBytes();
    const auto kBufferSize =
        parentBytesAndCount.second
        + sizeof(KeysCount);
    // Use BytesSerializer for consistent serialization
    BytesSerializer serializer;
    serializer.enqueue(parentBytesAndCount);
    serializer.copy(mKeysCount);

    return serializer.collect();
}