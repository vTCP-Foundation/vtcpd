#include "PublicKeyHashConfirmation.h"
#include "../../../common/serialization/BytesDeserializer.h"

PublicKeyHashConfirmation::PublicKeyHashConfirmation(
    const SerializedEquivalent equivalent,
    Contractor::Shared contractor,
    const TransactionUUID &transactionUUID,
    sphincs::KeyHash::Shared hashConfirmation):
    ConfirmationMessage(
        equivalent,
        contractor->ownIdOnContractorSide(),
        transactionUUID),
    mHashConfirmation(hashConfirmation)
{
    encrypt(contractor);
}

PublicKeyHashConfirmation::PublicKeyHashConfirmation(
    const SerializedEquivalent equivalent,
    Contractor::Shared contractor,
    const TransactionUUID &transactionUUID,
    OperationState state) :
    ConfirmationMessage(
        equivalent,
        contractor->ownIdOnContractorSide(),
        transactionUUID,
        state),
    mHashConfirmation(nullptr)
{
    encrypt(contractor);
}

PublicKeyHashConfirmation::PublicKeyHashConfirmation(
    BytesShared buffer) :
    ConfirmationMessage(buffer)
{
    auto bytesBufferOffset = ConfirmationMessage::kOffsetToInheritedBytes();

    if (state() == ConfirmationMessage::OK) {
        BytesDeserializer deserializer(buffer, bytesBufferOffset);

        // Get current offset for crypto key creation
        auto currentOffset = deserializer.getCurrentOffset();
        mHashConfirmation = make_shared<sphincs::KeyHash>(buffer.get() + currentOffset);
        deserializer.skipBytes(sphincs::KeyHash::kBytesSize);
    }
}

const Message::MessageType PublicKeyHashConfirmation::typeID() const
{
    return Message::TrustLines_HashConfirmation;
}

const sphincs::KeyHash::Shared PublicKeyHashConfirmation::hashConfirmation() const
{
    return mHashConfirmation;
}

pair<BytesShared, size_t> PublicKeyHashConfirmation::serializeToBytes() const
{
    // TODO: Performance optimization - parent data is serialized twice:
    // 1. ConfirmationMessage::serializeToBytes() creates buffer
    // 2. serializer.enqueue() copies that buffer again
    // Consider passing serializer down inheritance chain to avoid redundant allocation/copy
    const auto parentBytesAndCount = ConfirmationMessage::serializeToBytes();
    auto kBufferSize = parentBytesAndCount.second;
    if (state() == ConfirmationMessage::OK) {
        kBufferSize += sphincs::KeyHash::kBytesSize;
    }
    // Use BytesSerializer for consistent serialization
    BytesSerializer serializer;
    serializer.enqueue(parentBytesAndCount);

    if (state() == ConfirmationMessage::OK) {
        serializer.copy(mHashConfirmation->data(), sphincs::KeyHash::kBytesSize);
    }

    return serializer.collect();
}