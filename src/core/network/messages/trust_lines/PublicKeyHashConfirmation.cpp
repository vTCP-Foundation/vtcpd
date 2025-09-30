#include "PublicKeyHashConfirmation.h"

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
        mHashConfirmation = make_shared<sphincs::KeyHash>(
                                buffer.get() + bytesBufferOffset);
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
    const auto parentBytesAndCount = ConfirmationMessage::serializeToBytes();
    auto kBufferSize = parentBytesAndCount.second;
    if (state() == ConfirmationMessage::OK) {
        kBufferSize += sphincs::KeyHash::kBytesSize;
    }
    BytesShared buffer = tryMalloc(kBufferSize);

    size_t dataBytesOffset = 0;
    // Parent message content
    memcpy(
        buffer.get(),
        parentBytesAndCount.first.get(),
        parentBytesAndCount.second);
    dataBytesOffset += parentBytesAndCount.second;

    if (state() == ConfirmationMessage::OK) {
        memcpy(
            buffer.get() + dataBytesOffset,
            mHashConfirmation->data(),
            sphincs::KeyHash::kBytesSize);
    }

    return make_pair(
               buffer,
               kBufferSize);
}