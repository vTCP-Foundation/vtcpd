#include "RequestMessageWithReservations.h"
#include "../../../../common/serialization/BytesDeserializer.h"

RequestMessageWithReservations::RequestMessageWithReservations(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    const vector<pair<PathID, ConstSharedTrustLineAmount>> &finalAmountsConfig) :

    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID),

    mFinalAmountsConfiguration(finalAmountsConfig)
{
}

RequestMessageWithReservations::RequestMessageWithReservations(
    const SerializedEquivalent equivalent,
    ContractorID contractorID,
    const TransactionUUID &transactionUUID,
    const vector<pair<PathID, ConstSharedTrustLineAmount>> &finalAmountsConfig) :

    TransactionMessage(
        equivalent,
        contractorID,
        transactionUUID),

    mFinalAmountsConfiguration(finalAmountsConfig)
{
}

RequestMessageWithReservations::RequestMessageWithReservations(
    BytesShared buffer) :

    TransactionMessage(buffer)
{
    auto currentOffset = TransactionMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, currentOffset);

    SerializedRecordsCount finalAmountsConfigurationCount;
    deserializer.copyInto(&finalAmountsConfigurationCount);

    mFinalAmountsConfiguration.reserve(finalAmountsConfigurationCount);
    for (SerializedRecordNumber idx = 0; idx < finalAmountsConfigurationCount; idx++) {
        PathID pathID;
        deserializer.copyInto(&pathID);

        TrustLineAmount trustLineAmount;
        deserializer.copyInto(&trustLineAmount);

        mFinalAmountsConfiguration.emplace_back(
            pathID,
            make_shared<const TrustLineAmount>(trustLineAmount));
    }
}

const vector<pair<PathID, ConstSharedTrustLineAmount>> &RequestMessageWithReservations::finalAmountsConfiguration() const {
    return mFinalAmountsConfiguration;
}

pair<BytesShared, size_t> RequestMessageWithReservations::serializeToBytes() const
{
    // TODO: Serialization architecture optimization needed across entire inheritance chain:
    // This base class pattern causes redundant parent serialization in all child classes.
    // Each child calls parent::serializeToBytes() then copies that buffer again via enqueue().
    // Consider: void serializeToSerializer(BytesSerializer& serializer) pattern to avoid copies.
    BytesSerializer serializer;

    serializer.enqueue(TransactionMessage::serializeToBytes());

    auto finalAmountsConfigurationCount = (SerializedRecordsCount)mFinalAmountsConfiguration.size();
    serializer.copy(finalAmountsConfigurationCount);

    for (auto const &it : mFinalAmountsConfiguration) {
        serializer.copy(it.first);
        serializer.copy(*it.second.get());
    }

    return serializer.collect();
}

const size_t RequestMessageWithReservations::kOffsetToInheritedBytes() const
{
    auto kOffset =
        TransactionMessage::kOffsetToInheritedBytes() +
        BytesSerializer::kSerializedRecordsCountSize +
        finalAmountsConfiguration().size() * (BytesSerializer::kSerializedPathIDSize + BytesSerializer::kSerializedTrustLineAmountSize);

    return kOffset;
}
