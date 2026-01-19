#include "AuditResponseMessage.h"
#include "AuditMessage.h"
#include "../../common/serialization/BytesDeserializer.h"
#include "../../common/serialization/BytesSerializer.h"

AuditResponseMessage::AuditResponseMessage(
    const SerializedEquivalent equivalent,
    Contractor::Shared contractor,
    const TransactionUUID &transactionUUID,
    const sphincs::Signature::Shared signature,
    const vector<TransactionUUID> &transactionUUIDs):
    ConfirmationMessage(
        equivalent,
        contractor->ownIdOnContractorSide(),
        transactionUUID),
    mTransactionUUIDs(transactionUUIDs),
    mSignature(signature)
{
    // Normalize list to enforce deterministic ordering and uniqueness.
    mTransactionUUIDs = AuditMessage::normalizedTransactionUUIDs(mTransactionUUIDs);
    encrypt(contractor);
}

AuditResponseMessage::AuditResponseMessage(
    const SerializedEquivalent equivalent,
    Contractor::Shared contractor,
    const TransactionUUID &transactionUUID,
    OperationState state,
    const vector<TransactionUUID> &transactionUUIDs) :
    ConfirmationMessage(
        equivalent,
        contractor->ownIdOnContractorSide(),
        transactionUUID,
        state),
    mTransactionUUIDs(transactionUUIDs),
    mSignature(nullptr)
{
    // Normalize list to enforce deterministic ordering and uniqueness.
    mTransactionUUIDs = AuditMessage::normalizedTransactionUUIDs(mTransactionUUIDs);
    encrypt(contractor);
}

AuditResponseMessage::AuditResponseMessage(
    BytesShared buffer) :
    ConfirmationMessage(buffer),
    mTransactionUUIDs(),
    mSignature(nullptr)
{
    BytesDeserializer deserializer(
        buffer,
        ConfirmationMessage::kOffsetToInheritedBytes());

    if (state() == ConfirmationMessage::OK) {
        // Read signature payload for successful audit response.
        mSignature = make_shared<sphincs::Signature>(
            buffer.get() + deserializer.getCurrentOffset());
        deserializer.skipBytes(sphincs::Signature::signatureSize());
    }

    uint32_t transactionUUIDsCount = 0;
    deserializer.copyInto(&transactionUUIDsCount);
    mTransactionUUIDs.reserve(transactionUUIDsCount);

    // Read UUID list payload after the optional signature.
    for (uint32_t idx = 0; idx < transactionUUIDsCount; ++idx) {
        TransactionUUID transactionUUID(
            buffer.get() + deserializer.getCurrentOffset());
        deserializer.skipBytes(TransactionUUID::kBytesSize);
        mTransactionUUIDs.push_back(transactionUUID);
    }

    // Normalize list to ensure deterministic ordering and uniqueness.
    mTransactionUUIDs = AuditMessage::normalizedTransactionUUIDs(mTransactionUUIDs);
}

const Message::MessageType AuditResponseMessage::typeID() const
{
    return Message::TrustLines_AuditConfirmation;
}

const vector<TransactionUUID>& AuditResponseMessage::transactionUUIDs() const
{
    return mTransactionUUIDs;
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

    // Serialize normalized UUID list after the optional signature.
    serializer.copy(static_cast<uint32_t>(mTransactionUUIDs.size()));
    for (const auto &transactionUUID : mTransactionUUIDs) {
        serializer.copy(transactionUUID);
    }

    return serializer.collect();
}
