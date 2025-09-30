#include "PublicKeyMessage.h"

PublicKeyMessage::PublicKeyMessage(
    const SerializedEquivalent equivalent,
    Contractor::Shared contractor,
    const TransactionUUID &transactionUUID,
    const crypto::sphincs::PublicKey::Shared publicKey):
    TransactionMessage(
        equivalent,
        contractor->ownIdOnContractorSide(),
        transactionUUID),
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

    auto publicKey = make_shared<crypto::sphincs::PublicKey>(
                         buffer.get() + bytesBufferOffset);
    mPublicKey = publicKey;
}

const Message::MessageType PublicKeyMessage::typeID() const
{
    return Message::TrustLines_PublicKey;
}

const crypto::sphincs::PublicKey::Shared PublicKeyMessage::publicKey() const
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
        + mPublicKey->keySize();
    BytesShared buffer = tryMalloc(kBufferSize);

    size_t dataBytesOffset = 0;
    // Parent message content
    memcpy(
        buffer.get(),
        parentBytesAndCount.first.get(),
        parentBytesAndCount.second);
    dataBytesOffset += parentBytesAndCount.second;

    memcpy(
        buffer.get() + dataBytesOffset,
        mPublicKey->data(),
        mPublicKey->keySize());

    return make_pair(
               buffer,
               kBufferSize);
}

const size_t PublicKeyMessage::kOffsetToInheritedBytes() const
{
    const auto kOffset =
        TransactionMessage::kOffsetToInheritedBytes()
        + mPublicKey->keySize();
    return kOffset;
}