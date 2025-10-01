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
    mOwnSignature(nullptr),
    mContractorSignature(nullptr)
{
}

AuditRecord::AuditRecord(
    AuditNumber auditNumber,
    TrustLineAmount &incomingAmount,
    TrustLineAmount &outgoingAmount,
    TrustLineBalance &balance,
    sphincs::Signature::Shared ownSignature,
    sphincs::Signature::Shared contractorSignature) :

    mAuditNumber(auditNumber),
    mIncomingAmount(incomingAmount),
    mOutgoingAmount(outgoingAmount),
    mBalance(balance),
    mOwnSignature(ownSignature),
    mContractorSignature(contractorSignature)
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

    mOwnSignature = make_shared<sphincs::Signature>(
                        buffer + bytesBufferOffset);
    bytesBufferOffset += sphincs::Signature::signatureSize();

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

const sphincs::Signature::Shared AuditRecord::ownSignature() const
{
    return mOwnSignature;
}

const sphincs::Signature::Shared AuditRecord::contractorSignature() const
{
    return mContractorSignature;
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
        mOwnSignature->data(),
        sphincs::Signature::signatureSize());
    dataBytesOffset += sphincs::Signature::signatureSize();

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        mContractorSignature->data(),
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
    return sizeof(AuditNumber) + kTrustLineAmountBytesCount + kTrustLineAmountBytesCount + kTrustLineBalanceSerializeBytesCount + sphincs::Signature::signatureSize() + sphincs::Signature::signatureSize();
}

const size_t AuditRecord::recordSizeForSignatureChecking()
{
    return sizeof(AuditNumber) + kTrustLineAmountBytesCount + kTrustLineAmountBytesCount + kTrustLineBalanceSerializeBytesCount;
}