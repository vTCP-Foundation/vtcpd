#include "RequestMessage.h"
#include "../../../../common/serialization/BytesDeserializer.h"

RequestMessage::RequestMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    const PathID &pathID,
    const TrustLineAmount &amount) :

    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID),
    mPathID(pathID),
    mAmount(amount)
{
}

RequestMessage::RequestMessage(
    BytesShared buffer) :

    TransactionMessage(buffer)
{
    auto currentOffset = TransactionMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, currentOffset);

    deserializer.copyInto(&mPathID);
    currentOffset += BytesSerializer::kSerializedPathIDSize;

    deserializer = BytesDeserializer(buffer, currentOffset);
    deserializer.copyInto(&mAmount);
}

const TrustLineAmount &RequestMessage::amount() const
{
    return mAmount;
}

const PathID &RequestMessage::pathID() const
{
    return mPathID;
}

pair<BytesShared, size_t> RequestMessage::serializeToBytes() const
{
    // TODO: Serialization architecture optimization needed across entire inheritance chain:
    // This base class pattern causes redundant parent serialization in all child classes.
    // Each child calls parent::serializeToBytes() then copies that buffer again via enqueue().
    // Consider: void serializeToSerializer(BytesSerializer& serializer) pattern to avoid copies.
    BytesSerializer serializer;

    serializer.enqueue(TransactionMessage::serializeToBytes());
    serializer.copy(mPathID);
    serializer.copy(mAmount);

    return serializer.collect();
}

const size_t RequestMessage::kOffsetToInheritedBytes() const
{
    const size_t offset =
        TransactionMessage::kOffsetToInheritedBytes() +
        BytesSerializer::kSerializedPathIDSize +
        BytesSerializer::kSerializedTrustLineAmountSize;

    return offset;
}
