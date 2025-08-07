#include "AuditResponseMessage.h"

AuditResponseMessage::AuditResponseMessage(
    const SerializedEquivalent equivalent,
    Contractor::Shared contractor,
    const TransactionUUID &transactionUUID,
    const sphincs::Signature::Shared signature):
    ConfirmationMessage(
        equivalent,
        contractor->ownIdOnContractorSide(),
        transactionUUID),
    mSignature(signature)
{
    encrypt(contractor);
}

AuditResponseMessage::AuditResponseMessage(
    const SerializedEquivalent equivalent,
    Contractor::Shared contractor,
    const TransactionUUID &transactionUUID,
    OperationState state) :
    ConfirmationMessage(
        equivalent,
        contractor->ownIdOnContractorSide(),
        transactionUUID,
        state),
    mSignature(nullptr)
{
    encrypt(contractor);
}

AuditResponseMessage::AuditResponseMessage(
    BytesShared buffer) :
    ConfirmationMessage(buffer)
{
    auto bytesBufferOffset = ConfirmationMessage::kOffsetToInheritedBytes();

    if (state() == ConfirmationMessage::OK) {
        mSignature = make_shared<sphincs::Signature>(
                         buffer.get() + bytesBufferOffset);
    }
}

const Message::MessageType AuditResponseMessage::typeID() const
{
    return Message::TrustLines_AuditConfirmation;
}

const sphincs::Signature::Shared AuditResponseMessage::signature() const
{
    return mSignature;
}

pair<BytesShared, size_t> AuditResponseMessage::serializeToBytes() const
{
    const auto parentBytesAndCount = ConfirmationMessage::serializeToBytes();
    auto kBufferSize = parentBytesAndCount.second;
    if (state() == ConfirmationMessage::OK) {
        kBufferSize += mSignature->signatureSize();
    }
    BytesShared buffer = tryMalloc(kBufferSize);

    size_t dataBytesOffset = 0;
    // Parent message content
    memcpy(
        buffer.get(),
        parentBytesAndCount.first.get(),
        parentBytesAndCount.second);

    if (state() == ConfirmationMessage::OK) {
        dataBytesOffset += parentBytesAndCount.second;

        memcpy(
            buffer.get() + dataBytesOffset,
            mSignature->data(),
            mSignature->signatureSize());
    }

    return make_pair(
               buffer,
               kBufferSize);
}