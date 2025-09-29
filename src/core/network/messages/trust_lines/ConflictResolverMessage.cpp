#include "ConflictResolverMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"
#include "../../../common/serialization/BytesSerializer.h"

ConflictResolverMessage::ConflictResolverMessage(
    const SerializedEquivalent equivalent,
    ContractorID idOnReceiverSide,
    const TransactionUUID &transactionUUID,
    AuditRecord::Shared auditRecord,
    vector<ReceiptRecord::Shared> &incomingReceipts,
    vector<ReceiptRecord::Shared> &outgoingReceipts):

    TransactionMessage(
        equivalent,
        idOnReceiverSide,
        transactionUUID),
    mAuditRecord(auditRecord),
    mIncomingReceipts(incomingReceipts),
    mOutgoingReceipts(outgoingReceipts)
{}

ConflictResolverMessage::ConflictResolverMessage(
    BytesShared buffer) :
    TransactionMessage(buffer)
{
    auto currentOffset = TransactionMessage::kOffsetToInheritedBytes();

    mAuditRecord = make_shared<AuditRecord>(
                       buffer.get() + currentOffset);
    currentOffset += AuditRecord::recordSize();

    BytesDeserializer deserializer(buffer, currentOffset);
    SerializedRecordsCount incomingReceiptsCount;
    deserializer.copyInto(&incomingReceiptsCount);
    currentOffset += sizeof(SerializedRecordsCount);
    mIncomingReceipts.reserve(incomingReceiptsCount);

    for (SerializedRecordNumber idx = 0; idx < incomingReceiptsCount; idx++) {
        auto incomingReceiptRecord = make_shared<ReceiptRecord>(
                                         buffer.get() + currentOffset);
        currentOffset += ReceiptRecord::recordSize();
        mIncomingReceipts.push_back(
            incomingReceiptRecord);
    }

    SerializedRecordsCount outgoingReceiptsCount;
    deserializer = BytesDeserializer(buffer, currentOffset);
    deserializer.copyInto(&outgoingReceiptsCount);
    currentOffset += sizeof(SerializedRecordsCount);
    mOutgoingReceipts.reserve(outgoingReceiptsCount);

    for (SerializedRecordNumber idx = 0; idx < outgoingReceiptsCount; idx++) {
        auto outgoingReceiptRecord = make_shared<ReceiptRecord>(
                                         buffer.get() + currentOffset);
        currentOffset += ReceiptRecord::recordSize();
        mOutgoingReceipts.push_back(
            outgoingReceiptRecord);
    }
}

const Message::MessageType ConflictResolverMessage::typeID() const
{
    return Message::TrustLines_ConflictResolver;
}


AuditRecord::Shared ConflictResolverMessage::auditRecord() const
{
    return mAuditRecord;
}

const vector<ReceiptRecord::Shared> ConflictResolverMessage::incomingReceipts() const
{
    return mIncomingReceipts;
}

const vector<ReceiptRecord::Shared> ConflictResolverMessage::outgoingReceipts() const
{
    return mOutgoingReceipts;
}

const bool ConflictResolverMessage::isAddToConfirmationRequiredMessagesHandler() const
{
    return true;
}

const bool ConflictResolverMessage::isCheckCachedResponse() const
{
    return true;
}

pair<BytesShared, size_t> ConflictResolverMessage::serializeToBytes() const
{
    BytesSerializer serializer;
    serializer.enqueue(TransactionMessage::serializeToBytes());

    auto serializedAuditRecord = mAuditRecord->serializeToBytes();
    serializer.copy(
        serializedAuditRecord.get(),
        AuditRecord::recordSize());

    serializer.copy((SerializedRecordsCount)mIncomingReceipts.size());

    for (const auto &incomingReceiptRecord : mIncomingReceipts) {
        serializer.copy(
            incomingReceiptRecord->serializeToBytes().get(),
            ReceiptRecord::recordSize());
    }

    serializer.copy((SerializedRecordsCount)mOutgoingReceipts.size());

    for (const auto &outgoingReceiptRecord : mOutgoingReceipts) {
        serializer.copy(
            outgoingReceiptRecord->serializeToBytes().get(),
            ReceiptRecord::recordSize());
    }

    return serializer.collect();
}