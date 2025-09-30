#include "TransactionPublicKeyHashMessage.h"

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

    memcpy(
        &mPaymentNodeID,
        buffer.get() + bytesBufferOffset,
        sizeof(PaymentNodeID));
    bytesBufferOffset += sizeof(PaymentNodeID);

    mTransactionPublicKeyHash = make_shared<sphincs::KeyHash>(
        buffer.get() + bytesBufferOffset);
    bytesBufferOffset += sphincs::KeyHash::kBytesSize;

    memcpy(
        &mIsReceiptContains,
        buffer.get() + bytesBufferOffset,
        sizeof(byte_t));

    if (mIsReceiptContains) {
        bytesBufferOffset += sizeof(byte_t);
        auto signature = make_shared<sphincs::Signature>(
                             buffer.get() + bytesBufferOffset);
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
    const auto parentBytesAndCount = TransactionMessage::serializeToBytes();

    auto kBufferSize =
        parentBytesAndCount.second + sizeof(PaymentNodeID) + sphincs::KeyHash::kBytesSize + sizeof(byte_t);
    if (mIsReceiptContains) {
        kBufferSize += sphincs::Signature::signatureSize();
    }

    BytesShared buffer = tryMalloc(kBufferSize);

    size_t dataBytesOffset = 0;
    memcpy(
        buffer.get(),
        parentBytesAndCount.first.get(),
        parentBytesAndCount.second);
    dataBytesOffset += parentBytesAndCount.second;

    memcpy(
        buffer.get() + dataBytesOffset,
        &mPaymentNodeID,
        sizeof(PaymentNodeID));
    dataBytesOffset += sizeof(PaymentNodeID);

    memcpy(
        buffer.get() + dataBytesOffset,
        mTransactionPublicKeyHash->data(),
        sphincs::KeyHash::kBytesSize);
    dataBytesOffset += sphincs::KeyHash::kBytesSize;

    memcpy(
        buffer.get() + dataBytesOffset,
        &mIsReceiptContains,
        sizeof(byte_t));

    if (mIsReceiptContains) {
        dataBytesOffset += sizeof(byte_t);
        memcpy(
            buffer.get() + dataBytesOffset,
            mSignature->data(),
            sphincs::Signature::signatureSize());
    }

    return make_pair(
               buffer,
               kBufferSize);
}