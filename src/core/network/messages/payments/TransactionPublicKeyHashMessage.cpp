#include "TransactionPublicKeyHashMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"

TransactionPublicKeyHashMessage::TransactionPublicKeyHashMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    const PaymentNodeID paymentNodeID,
    const sphincs::KeyHash::Shared transactionPublicKeyHash) :

    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID),
    mPaymentNodeID(paymentNodeID),
    mTransactionPublicKeyHash(transactionPublicKeyHash),
    mIsReceiptContains(false)
{
}

TransactionPublicKeyHashMessage::TransactionPublicKeyHashMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    const PaymentNodeID paymentNodeID,
    const sphincs::KeyHash::Shared transactionPublicKeyHash,
    const sphincs::Signature::Shared signature) :

    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID),
    mPaymentNodeID(paymentNodeID),
    mTransactionPublicKeyHash(transactionPublicKeyHash),
    mIsReceiptContains(true),
    mSignature(signature)
{
}

TransactionPublicKeyHashMessage::TransactionPublicKeyHashMessage(
    BytesShared buffer) : TransactionMessage(buffer)
{
    auto bytesBufferOffset = TransactionMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, bytesBufferOffset);

    deserializer.copyInto(&mPaymentNodeID);

    // Get current offset for crypto key creation
    auto currentOffset = deserializer.getCurrentOffset();
    mTransactionPublicKeyHash = make_shared<sphincs::KeyHash>(buffer.get() + currentOffset);
    deserializer.skipBytes(sphincs::KeyHash::kBytesSize);

    deserializer.copyInto(&mIsReceiptContains);

    if (mIsReceiptContains) {
        // Get current offset for signature creation
        currentOffset = deserializer.getCurrentOffset();
        auto signature = make_shared<sphincs::Signature>(buffer.get() + currentOffset);
        mSignature = signature;
    }
}

const Message::MessageType TransactionPublicKeyHashMessage::typeID() const
{
    return Message::Payments_TransactionPublicKeyHash;
}

const PaymentNodeID TransactionPublicKeyHashMessage::paymentNodeID() const
{
    return mPaymentNodeID;
}

const sphincs::KeyHash::Shared TransactionPublicKeyHashMessage::transactionPublicKeyHash() const
{
    return mTransactionPublicKeyHash;
}

bool TransactionPublicKeyHashMessage::isReceiptContains() const
{
    return mIsReceiptContains;
}

const sphincs::Signature::Shared TransactionPublicKeyHashMessage::signature() const
{
    return mSignature;
}

pair<BytesShared, size_t> TransactionPublicKeyHashMessage::serializeToBytes() const
{
    // TODO: Performance optimization - parent data is serialized twice:
    // 1. TransactionMessage::serializeToBytes() creates buffer
    // 2. serializer.enqueue() copies that buffer again
    // Consider passing serializer down inheritance chain to avoid redundant allocation/copy
    const auto parentBytesAndCount = TransactionMessage::serializeToBytes();

    auto kBufferSize =
        parentBytesAndCount.second + sizeof(PaymentNodeID) + sphincs::KeyHash::kBytesSize + sizeof(byte_t);
    if (mIsReceiptContains) {
        kBufferSize += sphincs::Signature::signatureSize();
    }

    // Use BytesSerializer for consistent serialization
    BytesSerializer serializer;
    serializer.enqueue(parentBytesAndCount);
    serializer.copy(mPaymentNodeID);
    serializer.copy(mTransactionPublicKeyHash->data(), sphincs::KeyHash::kBytesSize);
    serializer.copy(mIsReceiptContains);

    if (mIsReceiptContains) {
        serializer.copy(mSignature->data(), mSignature->signatureSize());
    }

    return serializer.collect();
}