#include "TrustLineResetMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"

TrustLineResetMessage::TrustLineResetMessage(
    const SerializedEquivalent equivalent,
    Contractor::Shared contractor,
    const TransactionUUID &transactionUUID,
    const AuditNumber auditNumber,
    const TrustLineAmount &incomingAmount,
    const TrustLineAmount &outgoingAmount,
    const TrustLineBalance &balance):
    TransactionMessage(
        equivalent,
        contractor->ownIdOnContractorSide(),
        transactionUUID),
    mAuditNumber(auditNumber),
    mIncomingAmount(incomingAmount),
    mOutgoingAmount(outgoingAmount),
    mBalance(balance)
{
    encrypt(contractor);
}

TrustLineResetMessage::TrustLineResetMessage(
    BytesShared buffer) :
    TransactionMessage(buffer)
{
    auto bytesBufferOffset = TransactionMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, bytesBufferOffset);

    deserializer.copyInto(&mAuditNumber);
    deserializer.copyInto(&mIncomingAmount);
    deserializer.copyInto(&mOutgoingAmount);
    bytesBufferOffset += kTrustLineAmountBytesCount;

    vector<byte_t> balance(
        buffer.get() + bytesBufferOffset,
        buffer.get() + bytesBufferOffset + kTrustLineBalanceSerializeBytesCount);
    mBalance = bytesToTrustLineBalance(balance);
}

const Message::MessageType TrustLineResetMessage::typeID() const
{
    return Message::TrustLines_Reset;
}

const AuditNumber TrustLineResetMessage::auditNumber() const
{
    return mAuditNumber;
}

const TrustLineAmount& TrustLineResetMessage::incomingAmount() const
{
    return mIncomingAmount;
}

const TrustLineAmount& TrustLineResetMessage::outgoingAmount() const
{
    return mOutgoingAmount;
}

const TrustLineBalance& TrustLineResetMessage::balance() const
{
    return mBalance;
}

const bool TrustLineResetMessage::isCheckCachedResponse() const
{
    return true;
}

pair<BytesShared, size_t> TrustLineResetMessage::serializeToBytes() const
{
    // TODO: Performance optimization - parent data is serialized twice:
    // 1. TransactionMessage::serializeToBytes() creates buffer
    // 2. serializer.enqueue() copies that buffer again
    // Consider passing serializer down inheritance chain to avoid redundant allocation/copy
    const auto parentBytesAndCount = TransactionMessage::serializeToBytes();
    auto kBufferSize = parentBytesAndCount.second
                       + sizeof(AuditNumber)
                       + kTrustLineAmountBytesCount
                       + kTrustLineAmountBytesCount
                       + kTrustLineBalanceSerializeBytesCount;
    // Use BytesSerializer for consistent serialization
    BytesSerializer serializer;
    serializer.enqueue(parentBytesAndCount);
    serializer.copy(mAuditNumber);
    serializer.copy(mIncomingAmount);
    serializer.copy(mOutgoingAmount);

    vector<byte_t> balanceBuffer = trustLineBalanceToBytes(mBalance);
    serializer.copy(balanceBuffer.data(), balanceBuffer.size());

    return serializer.collect();
}