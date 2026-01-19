#include "AuditMessage.h"
#include "../../common/serialization/BytesDeserializer.h"
#include "../../common/serialization/BytesSerializer.h"
#include "../../../common/memory/MemoryUtils.h"

#include <algorithm>
#include <cstring>
#include <openssl/sha.h>

AuditMessage::AuditMessage(
    const SerializedEquivalent equivalent,
    Contractor::Shared contractor,
    const TransactionUUID &transactionUUID,
    const AuditNumber auditNumber,
    const TrustLineAmount &incomingAmount,
    const TrustLineAmount &outgoingAmount,
    const sphincs::Signature::Shared signature,
    const vector<TransactionUUID> &transactionUUIDs) : TransactionMessage(equivalent,
                contractor->ownIdOnContractorSide(),
                transactionUUID),
    mAuditNumber(auditNumber),
    mIncomingAmount(incomingAmount),
    mOutgoingAmount(outgoingAmount),
    mTransactionUUIDs(transactionUUIDs),
    mSignature(signature)
{
    // Normalize list to enforce deterministic ordering and uniqueness.
    mTransactionUUIDs = normalizedTransactionUUIDs(mTransactionUUIDs);
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

    uint32_t transactionUUIDsCount = 0;
    deserializer.copyInto(&transactionUUIDsCount);
    mTransactionUUIDs.reserve(transactionUUIDsCount);

    // Read UUID list payload before the signature.
    for (uint32_t idx = 0; idx < transactionUUIDsCount; ++idx) {
        TransactionUUID transactionUUID(
            buffer.get() + deserializer.getCurrentOffset());
        deserializer.skipBytes(TransactionUUID::kBytesSize);
        mTransactionUUIDs.push_back(transactionUUID);
    }

    // Normalize list to ensure deterministic ordering and uniqueness.
    mTransactionUUIDs = normalizedTransactionUUIDs(mTransactionUUIDs);

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

const vector<TransactionUUID>& AuditMessage::transactionUUIDs() const
{
    return mTransactionUUIDs;
}

const sphincs::Signature::Shared AuditMessage::signature() const
{
    return mSignature;
}

void AuditMessage::sortTransactionUUIDs(
    vector<TransactionUUID> &transactionUUIDs)
{
    // Sort UUIDs lexicographically by raw bytes for deterministic ordering.
    sort(
        transactionUUIDs.begin(),
        transactionUUIDs.end(),
        [](const TransactionUUID &left, const TransactionUUID &right) {
            return memcmp(
                left.data,
                right.data,
                TransactionUUID::kBytesSize) < 0;
        });
}

vector<TransactionUUID> AuditMessage::normalizedTransactionUUIDs(
    const vector<TransactionUUID> &transactionUUIDs)
{
    // Sort and deduplicate to enforce canonical ordering.
    vector<TransactionUUID> normalizedUUIDs = transactionUUIDs;
    sortTransactionUUIDs(normalizedUUIDs);

    const auto uniqueEnd = unique(
        normalizedUUIDs.begin(),
        normalizedUUIDs.end(),
        [](const TransactionUUID &left, const TransactionUUID &right) {
            return memcmp(
                left.data,
                right.data,
                TransactionUUID::kBytesSize) == 0;
        });
    normalizedUUIDs.erase(
        uniqueEnd,
        normalizedUUIDs.end());

    return normalizedUUIDs;
}

BytesShared AuditMessage::computeTransactionListHash(
    const vector<TransactionUUID> &transactionUUIDs)
{
    // Normalize list to enforce deterministic ordering and uniqueness.
    const auto normalizedUUIDs =
        normalizedTransactionUUIDs(transactionUUIDs);
    const uint32_t transactionUUIDsCount =
        static_cast<uint32_t>(normalizedUUIDs.size());

    // Hash count and concatenated UUID bytes in canonical order.
    const size_t bufferSize =
        sizeof(transactionUUIDsCount) +
        (normalizedUUIDs.size() * TransactionUUID::kBytesSize);
    auto buffer = tryMalloc(bufferSize);

    size_t offset = 0;
    memcpy(
        buffer.get() + offset,
        &transactionUUIDsCount,
        sizeof(transactionUUIDsCount));
    offset += sizeof(transactionUUIDsCount);

    for (const auto &transactionUUID : normalizedUUIDs) {
        memcpy(
            buffer.get() + offset,
            transactionUUID.data,
            TransactionUUID::kBytesSize);
        offset += TransactionUUID::kBytesSize;
    }

    auto hash = tryMalloc(SHA256_DIGEST_LENGTH);
    SHA256(buffer.get(), bufferSize, hash.get());
    return hash;
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
    // Serialize normalized UUID list before the signature.
    serializer.copy(static_cast<uint32_t>(mTransactionUUIDs.size()));
    for (const auto &transactionUUID : mTransactionUUIDs) {
        serializer.copy(transactionUUID);
    }
    serializer.copy(
        mSignature->data(),
        mSignature->signatureSize());

    return serializer.collect();
}

const size_t AuditMessage::kOffsetToInheritedBytes() const
{
    const auto kOffset =
        TransactionMessage::kOffsetToInheritedBytes() +
        sizeof(AuditNumber) +
        kTrustLineAmountBytesCount +
        kTrustLineAmountBytesCount +
        sizeof(uint32_t) +
        (mTransactionUUIDs.size() * TransactionUUID::kBytesSize) +
        mSignature->signatureSize();
    return kOffset;
}
