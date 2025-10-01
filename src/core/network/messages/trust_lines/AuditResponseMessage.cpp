#include "AuditResponseMessage.h"
#include "../../common/serialization/BytesDeserializer.h"
#include "../../common/serialization/BytesSerializer.h"

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
    if (state() == ConfirmationMessage::OK) {
        BytesDeserializer deserializer(
            buffer,
            ConfirmationMessage::kOffsetToInheritedBytes());

        mSignature = make_shared<sphincs::Signature>(
            buffer.get() + deserializer.getCurrentOffset());
        deserializer.skipBytes(sphincs::Signature::signatureSize());
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
    BytesSerializer serializer;

    serializer.enqueue(ConfirmationMessage::serializeToBytes());

    if (state() == ConfirmationMessage::OK) {
        serializer.copy(
            mSignature->data(),
            mSignature->signatureSize());
    }

    return serializer.collect();
}
