#include "ReceiptRecord.h"

ReceiptRecord::ReceiptRecord(
    const AuditNumber auditNumber,
    const TransactionUUID &transactionUUID,
    const TrustLineAmount &amount,
    const sphincs::KeyHash::Shared keyHash,
    const sphincs::Signature::Shared signature) : mAuditNumber(auditNumber),
    mTransactionUUID(transactionUUID),
    mAmount(amount),
    mKeyHash(keyHash),
    mSignature(signature)
{
}

ReceiptRecord::ReceiptRecord(
    byte_t* buffer)
{
    auto bytesBufferOffset = 0;
    memcpy(
        &mAuditNumber,
        buffer + bytesBufferOffset,
        sizeof(AuditNumber));
    bytesBufferOffset += sizeof(AuditNumber);

    memcpy(
        mTransactionUUID.data,
        buffer + bytesBufferOffset,
        TransactionUUID::kBytesSize);
    bytesBufferOffset += TransactionUUID::kBytesSize;

    vector<byte_t> amountBytes(
        buffer + bytesBufferOffset,
        buffer + bytesBufferOffset + kTrustLineAmountBytesCount);
    mAmount = bytesToTrustLineAmount(amountBytes);
    bytesBufferOffset += kTrustLineAmountBytesCount;

    mKeyHash = make_shared<sphincs::KeyHash>(
                   buffer + bytesBufferOffset);
    bytesBufferOffset += sphincs::KeyHash::kBytesSize;

    mSignature = make_shared<sphincs::Signature>(
                     buffer + bytesBufferOffset);
}

const AuditNumber ReceiptRecord::auditNumber() const
{
    return mAuditNumber;
}

const TransactionUUID &ReceiptRecord::transactionUUID() const
{
    return mTransactionUUID;
}

const TrustLineAmount &ReceiptRecord::amount() const
{
    return mAmount;
}

const sphincs::KeyHash::Shared ReceiptRecord::keyHash() const
{
    return mKeyHash;
}

const sphincs::Signature::Shared ReceiptRecord::signature() const
{
    return mSignature;
}

BytesShared ReceiptRecord::serializeToBytes()
{
    BytesShared dataBytesShared = tryCalloc(recordSize());
    size_t dataBytesOffset = 0;

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mAuditNumber,
        sizeof(AuditNumber));
    dataBytesOffset += sizeof(AuditNumber);

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        mTransactionUUID.data,
        TransactionUUID::kBytesSize);
    dataBytesOffset += TransactionUUID::kBytesSize;

    vector<byte_t> amountBufferBytes = trustLineAmountToBytes(
                                           mAmount);
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        amountBufferBytes.data(),
        kTrustLineAmountBytesCount);
    dataBytesOffset += kTrustLineAmountBytesCount;

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        mKeyHash->data(),
        sphincs::KeyHash::kBytesSize);
    dataBytesOffset += sphincs::KeyHash::kBytesSize;

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        mSignature->data(),
        sphincs::Signature::signatureSize());

    return dataBytesShared;
}

const size_t ReceiptRecord::recordSize()
{
    return sizeof(AuditNumber) + TransactionUUID::kBytesSize + kTrustLineAmountBytesCount + sphincs::KeyHash::kBytesSize + sphincs::Signature::signatureSize();
}

bool operator==(
    const ReceiptRecord::Shared record1,
    const ReceiptRecord::Shared record2)
{
    if (record1->auditNumber() != record2->auditNumber()) {
        return false;
    }
    if (record1->transactionUUID() != record2->transactionUUID()) {
        return false;
    }
    if (record1->amount() != record2->amount()) {
        return false;
    }
    if (*record1->keyHash() != *record2->keyHash()) {
        return false;
    }
    // todo compare signatures
    return true;
}