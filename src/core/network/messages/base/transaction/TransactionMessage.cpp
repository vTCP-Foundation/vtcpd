#include "TransactionMessage.h"


TransactionMessage::TransactionMessage(
    const SerializedEquivalent equivalent,
    const TransactionUUID &transactionUUID):

    SenderMessage(
        equivalent,
        0),
    mTransactionUUID(transactionUUID)
{}

TransactionMessage::TransactionMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID):

    SenderMessage(
        equivalent,
        senderAddresses),
    mTransactionUUID(transactionUUID)
{}

TransactionMessage::TransactionMessage(
    const SerializedEquivalent equivalent,
    ContractorID idOnReceiverSide,
    const TransactionUUID &transactionUUID):

    SenderMessage(
        equivalent,
        idOnReceiverSide),
    mTransactionUUID(transactionUUID)
{}

TransactionMessage::TransactionMessage(
    BytesShared buffer):

    SenderMessage(buffer),
    mTransactionUUID([&buffer](const size_t parentOffset) -> const TransactionUUID {
                         // Use the safe constructor from uint8_t* that we fixed in NodeUUID
                         return TransactionUUID(buffer.get() + parentOffset);
                     }(SenderMessage::kOffsetToInheritedBytes()))
{}

const bool TransactionMessage::isTransactionMessage() const
{
    return true;
}

const TransactionUUID &TransactionMessage::transactionUUID() const
{
    return mTransactionUUID;
}

pair<BytesShared, size_t> TransactionMessage::serializeToBytes() const
{
    // TODO: Serialization architecture optimization needed across entire inheritance chain:
    // This base class pattern causes redundant parent serialization in all child classes.
    // Each child calls parent::serializeToBytes() then copies that buffer again via enqueue().
    // Consider: void serializeToSerializer(BytesSerializer& serializer) pattern to avoid copies.
    BytesSerializer serializer;

    serializer.enqueue(SenderMessage::serializeToBytes());
    serializer.copy(mTransactionUUID);

    return serializer.collect();
}

const size_t TransactionMessage::kOffsetToInheritedBytes() const
{
    const auto kOffset =
        SenderMessage::kOffsetToInheritedBytes()
        + BytesSerializer::kSerializedUUIDSize;

    return kOffset;
}
