#include "AuditRecord.h"

AuditRecord::AuditRecord(
    AuditNumber auditNumber,
    TrustLineAmount &incomingAmount,
    TrustLineAmount &outgoingAmount,
    TrustLineBalance &balance) :

    mAuditNumber(auditNumber),
    mIncomingAmount(incomingAmount),
    mOutgoingAmount(outgoingAmount),
    mBalance(balance),
    mOwnKeyHash(nullptr),
    mOwnSignature(nullptr),
    mContractorKeyHash(nullptr),
    mContractorSignature(nullptr),
    mOwnKeysSetHash(nullptr),
    mContractorKeysSetHash(nullptr)
{
}

AuditRecord::AuditRecord(
    AuditNumber auditNumber,
    TrustLineAmount &incomingAmount,
    TrustLineAmount &outgoingAmount,
    TrustLineBalance &balance,
    sphincs::KeyHash::Shared ownKeyHash,
    sphincs::Signature::Shared ownSignature,
    sphincs::KeyHash::Shared contractorKeyHash,
    sphincs::Signature::Shared contractorSignature,
    sphincs::KeyHash::Shared ownKeysSetHash,
    sphincs::KeyHash::Shared contractorKeysSetHash) :

    mAuditNumber(auditNumber),
    mIncomingAmount(incomingAmount),
    mOutgoingAmount(outgoingAmount),
    mBalance(balance),
    mOwnKeyHash(ownKeyHash),
    mOwnSignature(ownSignature),
    mContractorKeyHash(contractorKeyHash),
    mContractorSignature(contractorSignature),
    mOwnKeysSetHash(ownKeysSetHash),
    mContractorKeysSetHash(contractorKeysSetHash)
{
}

AuditRecord::AuditRecord(
    byte_t* buffer)
{
    auto bytesBufferOffset = 0;
    memcpy(
        &mAuditNumber,
        buffer + bytesBufferOffset,
        sizeof(AuditNumber));
    bytesBufferOffset += sizeof(AuditNumber);

    vector<byte_t> incomingAmountBytes(
        buffer + bytesBufferOffset,
        buffer + bytesBufferOffset + kTrustLineAmountBytesCount);
    mIncomingAmount = bytesToTrustLineAmount(incomingAmountBytes);
    bytesBufferOffset += kTrustLineAmountBytesCount;

    vector<byte_t> outgoingAmountBytes(
        buffer + bytesBufferOffset,
        buffer + bytesBufferOffset + kTrustLineAmountBytesCount);
    mOutgoingAmount = bytesToTrustLineAmount(outgoingAmountBytes);
    bytesBufferOffset += kTrustLineAmountBytesCount;

    vector<byte_t> balanceBytes(
        buffer + bytesBufferOffset,
        buffer + bytesBufferOffset + kTrustLineBalanceSerializeBytesCount);
    mBalance = bytesToTrustLineBalance(balanceBytes);
    bytesBufferOffset += kTrustLineBalanceSerializeBytesCount;

    mOwnKeyHash = make_shared<sphincs::KeyHash>(
                      buffer + bytesBufferOffset);
    bytesBufferOffset += sphincs::KeyHash::kBytesSize;

    mOwnSignature = make_shared<sphincs::Signature>(
                        buffer + bytesBufferOffset);
    bytesBufferOffset += sphincs::Signature::signatureSize();

    mContractorKeyHash = make_shared<sphincs::KeyHash>(
                             buffer + bytesBufferOffset);
    bytesBufferOffset += sphincs::KeyHash::kBytesSize;

    mContractorSignature = make_shared<sphincs::Signature>(
                               buffer + bytesBufferOffset);
}

const AuditNumber AuditRecord::auditNumber() const
{
    return mAuditNumber;
}

const TrustLineAmount &AuditRecord::incomingAmount() const
{
    return mIncomingAmount;
}

const TrustLineAmount &AuditRecord::outgoingAmount() const
{
    return mOutgoingAmount;
}

const TrustLineBalance &AuditRecord::balance() const
{
    return mBalance;
}

const sphincs::KeyHash::Shared AuditRecord::ownKeyHash() const
{
    return mOwnKeyHash;
}

const sphincs::Signature::Shared AuditRecord::ownSignature() const
{
    return mOwnSignature;
}

const sphincs::KeyHash::Shared AuditRecord::contractorKeyHash() const
{
    return mContractorKeyHash;
}

const sphincs::Signature::Shared AuditRecord::contractorSignature() const
{
    return mContractorSignature;
}

const sphincs::KeyHash::Shared AuditRecord::ownKeysSetHash() const
{
    return mOwnKeysSetHash;
}

const sphincs::KeyHash::Shared AuditRecord::contractorKeysSetHash() const
{
    return mContractorKeysSetHash;
}

void AuditRecord::setContractorSignature(
    sphincs::Signature::Shared signature)
{
    mContractorSignature = signature;
}

bool AuditRecord::isPendingState() const
{
    return mContractorSignature == nullptr;
}

void AuditRecord::setOwnKeysSetHash(
    sphincs::KeyHash::Shared ownKeysSetHash)
{
    mOwnKeysSetHash = ownKeysSetHash;
}

void AuditRecord::setContractorKeysSetHash(
    sphincs::KeyHash::Shared contractorKeysSetHash)
{
    mContractorKeysSetHash = contractorKeysSetHash;
}

BytesShared AuditRecord::serializeToBytes()
{
    BytesShared dataBytesShared = tryCalloc(recordSize());
    size_t dataBytesOffset = 0;

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mAuditNumber,
        sizeof(AuditNumber));
    dataBytesOffset += sizeof(AuditNumber);

    vector<byte_t> incomingAmountBufferBytes = trustLineAmountToBytes(
            mIncomingAmount);
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        incomingAmountBufferBytes.data(),
        kTrustLineAmountBytesCount);
    dataBytesOffset += kTrustLineAmountBytesCount;

    vector<byte_t> outgoingAmountBufferBytes = trustLineAmountToBytes(
            mOutgoingAmount);
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        outgoingAmountBufferBytes.data(),
        kTrustLineAmountBytesCount);
    dataBytesOffset += kTrustLineAmountBytesCount;

    vector<byte_t> balanceBufferBytes = trustLineBalanceToBytes(
                                            const_cast<TrustLineBalance&>(mBalance));
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        balanceBufferBytes.data(),
        kTrustLineBalanceSerializeBytesCount);
    dataBytesOffset += kTrustLineBalanceSerializeBytesCount;

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        mOwnKeyHash->data(),
        sphincs::KeyHash::kBytesSize);
    dataBytesOffset += sphincs::KeyHash::kBytesSize;

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        mOwnSignature->data(),
        sphincs::Signature::signatureSize());
    dataBytesOffset += sphincs::Signature::signatureSize();

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        mContractorKeyHash->data(),
        sphincs::KeyHash::kBytesSize);
    dataBytesOffset += sphincs::KeyHash::kBytesSize;

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        mContractorKeyHash->data(),
        sphincs::Signature::signatureSize());

    return dataBytesShared;
}

BytesShared AuditRecord::serializeToCheckSignatureByInitiator()
{
    BytesShared dataBytesShared = tryCalloc(recordSizeForSignatureChecking());
    size_t dataBytesOffset = 0;

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mAuditNumber,
        sizeof(AuditNumber));
    dataBytesOffset += sizeof(AuditNumber);

    vector<byte_t> incomingAmountBufferBytes = trustLineAmountToBytes(
            mIncomingAmount);
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        incomingAmountBufferBytes.data(),
        kTrustLineAmountBytesCount);
    dataBytesOffset += kTrustLineAmountBytesCount;

    vector<byte_t> outgoingAmountBufferBytes = trustLineAmountToBytes(
            mOutgoingAmount);
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        outgoingAmountBufferBytes.data(),
        kTrustLineAmountBytesCount);
    dataBytesOffset += kTrustLineAmountBytesCount;

    vector<byte_t> balanceBufferBytes = trustLineBalanceToBytes(
                                            const_cast<TrustLineBalance&>(mBalance));
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        balanceBufferBytes.data(),
        kTrustLineBalanceSerializeBytesCount);

    return dataBytesShared;
}

BytesShared AuditRecord::serializeToCheckSignatureByContractor()
{
    BytesShared dataBytesShared = tryCalloc(recordSizeForSignatureChecking());
    size_t dataBytesOffset = 0;

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mAuditNumber,
        sizeof(AuditNumber));
    dataBytesOffset += sizeof(AuditNumber);

    vector<byte_t> outgoingAmountBufferBytes = trustLineAmountToBytes(
            mOutgoingAmount);
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        outgoingAmountBufferBytes.data(),
        kTrustLineAmountBytesCount);
    dataBytesOffset += kTrustLineAmountBytesCount;

    vector<byte_t> incomingAmountBufferBytes = trustLineAmountToBytes(
            mIncomingAmount);
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        incomingAmountBufferBytes.data(),
        kTrustLineAmountBytesCount);
    dataBytesOffset += kTrustLineAmountBytesCount;

    auto contractorBalance = -1 * mBalance;
    vector<byte_t> balanceBufferBytes = trustLineBalanceToBytes(
                                            const_cast<TrustLineBalance&>(contractorBalance));
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        balanceBufferBytes.data(),
        kTrustLineBalanceSerializeBytesCount);

    return dataBytesShared;
}

const size_t AuditRecord::recordSize()
{
    return sizeof(AuditNumber) + kTrustLineAmountBytesCount + kTrustLineAmountBytesCount + kTrustLineBalanceSerializeBytesCount + sphincs::KeyHash::kBytesSize + sphincs::Signature::signatureSize() + sphincs::KeyHash::kBytesSize + sphincs::Signature::signatureSize();
}

const size_t AuditRecord::recordSizeForSignatureChecking()
{
    return sizeof(AuditNumber) + kTrustLineAmountBytesCount + kTrustLineAmountBytesCount + kTrustLineBalanceSerializeBytesCount;
}