#include "AuditMessage.h"
#include "../../common/serialization/BytesDeserializer.h"
#include "../../common/serialization/BytesSerializer.h"

AuditMessage::AuditMessage(
    const SerializedEquivalent equivalent,
    Contractor::Shared contractor,
    const TransactionUUID &transactionUUID,
    const AuditNumber auditNumber,
    const TrustLineAmount &incomingAmount,
    const TrustLineAmount &outgoingAmount,
    const sphincs::Signature::Shared signature) : TransactionMessage(equivalent,
                contractor->ownIdOnContractorSide(),
                transactionUUID),
    mAuditNumber(auditNumber),
    mIncomingAmount(incomingAmount),
    mOutgoingAmount(outgoingAmount),
    mSignature(signature)
{
    encrypt(contractor);
}

AuditMessage::AuditMessage(
    BytesShared buffer) : TransactionMessage(buffer)
{
    BytesDeserializer deserializer(
        buffer,
        TransactionMessage::kOffsetToInheritedBytes());

    deserializer.copyInto(&mAuditNumber);
    deserializer.copyInto(&mIncomingAmount);
    deserializer.copyInto(&mOutgoingAmount);

    mSignature = make_shared<sphincs::Signature>(
        buffer.get() + deserializer.getCurrentOffset());
    deserializer.skipBytes(sphincs::Signature::signatureSize());
}

const Message::MessageType AuditMessage::typeID() const
{
    return Message::TrustLines_Audit;
}

const AuditNumber AuditMessage::auditNumber() const
{
    return mAuditNumber;
}

const TrustLineAmount &AuditMessage::incomingAmount() const
{
    return mIncomingAmount;
}

const TrustLineAmount &AuditMessage::outgoingAmount() const
{
    return mOutgoingAmount;
}

const sphincs::Signature::Shared AuditMessage::signature() const
{
    return mSignature;
}

const bool AuditMessage::isCheckCachedResponse() const
{
    return true;
}

pair<BytesShared, size_t> AuditMessage::serializeToBytes() const
{
    BytesSerializer serializer;

    serializer.enqueue(TransactionMessage::serializeToBytes());
    serializer.copy(mAuditNumber);
    serializer.copy(mIncomingAmount);
    serializer.copy(mOutgoingAmount);
    serializer.copy(
        mSignature->data(),
        mSignature->signatureSize());

    return serializer.collect();
}

const size_t AuditMessage::kOffsetToInheritedBytes() const
{
    const auto kOffset =
        TransactionMessage::kOffsetToInheritedBytes() + sizeof(AuditNumber) + kTrustLineAmountBytesCount + kTrustLineAmountBytesCount + mSignature->signatureSize();
    return kOffset;
}
