#include "PublicKeyMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"

PublicKeyMessage::PublicKeyMessage(
    const SerializedEquivalent equivalent,
    Contractor::Shared contractor,
    const TransactionUUID &transactionUUID,
    const KeyNumber number,
    const lamport::PublicKey::Shared publicKey):
    TransactionMessage(
        equivalent,
        contractor->ownIdOnContractorSide(),
        transactionUUID),
    mNumber(number),
    mPublicKey(publicKey)
{
    encrypt(contractor);
}

PublicKeyMessage::PublicKeyMessage(
    BytesShared buffer) :
    TransactionMessage(buffer),
    mPublicKey()
{
    auto bytesBufferOffset = TransactionMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, bytesBufferOffset);

    deserializer.copyInto(&mNumber);

    // Get current offset for crypto key creation
    auto currentOffset = deserializer.getCurrentOffset();
    auto publicKey = make_shared<lamport::PublicKey>(buffer.get() + currentOffset);
    deserializer.skipBytes(publicKey->keySize());
    mPublicKey = publicKey;
}

const Message::MessageType PublicKeyMessage::typeID() const
{
    return Message::TrustLines_PublicKey;
}

const KeyNumber PublicKeyMessage::number() const
{
    return mNumber;
}

const lamport::PublicKey::Shared PublicKeyMessage::publicKey() const
{
    return mPublicKey;
}

const bool PublicKeyMessage::isCheckCachedResponse() const
{
    return true;
}

pair<BytesShared, size_t> PublicKeyMessage::serializeToBytes() const
{
    const auto parentBytesAndCount = TransactionMessage::serializeToBytes();
    const auto kBufferSize =
        parentBytesAndCount.second
        + sizeof(KeyNumber)
        + mPublicKey->keySize();
    // Use BytesSerializer for consistent serialization
    BytesSerializer serializer;
    serializer.enqueue(parentBytesAndCount);
    serializer.copy(mNumber);
    serializer.copy(mPublicKey->data(), mPublicKey->keySize());

    return serializer.collect();
}

const size_t PublicKeyMessage::kOffsetToInheritedBytes() const
{
    const auto kOffset =
        TransactionMessage::kOffsetToInheritedBytes()
        + sizeof(KeyNumber)
        + mPublicKey->keySize();
    return kOffset;
}