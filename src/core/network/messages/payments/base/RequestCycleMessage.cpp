#include "RequestCycleMessage.h"
#include "../../../../common/serialization/BytesDeserializer.h"

RequestCycleMessage::RequestCycleMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    const TrustLineAmount &amount) :

    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID),
    mAmount(amount)
{
}

RequestCycleMessage::RequestCycleMessage(
    BytesShared buffer) :

    TransactionMessage(buffer)
{
    auto currentOffset = TransactionMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, currentOffset);

    deserializer.copyInto(&mAmount);
}

const TrustLineAmount &RequestCycleMessage::amount() const
{
    return mAmount;
}

pair<BytesShared, size_t> RequestCycleMessage::serializeToBytes() const
{
    // TODO: Serialization architecture optimization needed across entire inheritance chain:
    // This base class pattern causes redundant parent serialization in all child classes.
    // Each child calls parent::serializeToBytes() then copies that buffer again via enqueue().
    // Consider: void serializeToSerializer(BytesSerializer& serializer) pattern to avoid copies.
    BytesSerializer serializer;

    serializer.enqueue(TransactionMessage::serializeToBytes());
    serializer.copy(mAmount);

    return serializer.collect();
}

const size_t RequestCycleMessage::kOffsetToInheritedBytes() const
{
    const size_t offset =
        TransactionMessage::kOffsetToInheritedBytes() +
        BytesSerializer::kSerializedTrustLineAmountSize;

    return offset;
}
