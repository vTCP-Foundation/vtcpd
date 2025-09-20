#include "AuditResponseMessage.h"
#include "../../common/serialization/BytesDeserializer.h"
#include "../../common/serialization/BytesSerializer.h"

AuditResponseMessage::AuditResponseMessage(
    const SerializedEquivalent equivalent,
    Contractor::Shared contractor,
    const TransactionUUID &transactionUUID,
    const KeyNumber keyNumber,
    const lamport::Signature::Shared signature):
    ConfirmationMessage(
        equivalent,
        contractor->ownIdOnContractorSide(),
        transactionUUID),
    mSignature(signature),
    mKeyNumber(keyNumber)
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
    if (state() == ConfirmationMessage::OK) {
        BytesDeserializer deserializer(
            buffer,
            ConfirmationMessage::kOffsetToInheritedBytes());

        deserializer.copyInto(&mKeyNumber);

        mSignature = make_shared<lamport::Signature>(
            buffer.get() + deserializer.getCurrentOffset());
        deserializer.skipBytes(lamport::Signature::signatureSize());
    }
}

const Message::MessageType AuditResponseMessage::typeID() const
{
    return Message::TrustLines_AuditConfirmation;
}

const uint32_t AuditResponseMessage::keyNumber() const
{
    return mKeyNumber;
}

const lamport::Signature::Shared AuditResponseMessage::signature() const
{
    return mSignature;
}

pair<BytesShared, size_t> AuditResponseMessage::serializeToBytes() const
{
    BytesSerializer serializer;

    serializer.enqueue(ConfirmationMessage::serializeToBytes());

    if (state() == ConfirmationMessage::OK) {
        serializer.copy(mKeyNumber);
        serializer.copy(
            mSignature->data(),
            mSignature->signatureSize());
    }

    return serializer.collect();
}
