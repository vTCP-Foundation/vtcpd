#include "RequestMessageWithReservations.h"
#include "../../../../common/serialization/BytesDeserializer.h"

// NEW: Constructor with equivalents
RequestMessageWithReservations::RequestMessageWithReservations(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    const vector<PathReservation> &finalAmountsConfig) :

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
    const vector<PathReservation> &finalAmountsConfig) :

    TransactionMessage(
        equivalent,
        contractorID,
        transactionUUID),

    mFinalAmountsConfiguration(finalAmountsConfig)
{
}

// DEPRECATED: Constructor without equivalents (converts to PathReservation using mEquivalent)
RequestMessageWithReservations::RequestMessageWithReservations(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    const vector<pair<PathID, ConstSharedTrustLineAmount>> &finalAmountsConfig) :

    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID)
{
    // Convert pair vector to PathReservation vector using mEquivalent
    mFinalAmountsConfiguration.reserve(finalAmountsConfig.size());
    for (const auto& [pathID, amount] : finalAmountsConfig) {
        mFinalAmountsConfiguration.emplace_back(pathID, amount, equivalent);
    }
}

RequestMessageWithReservations::RequestMessageWithReservations(
    const SerializedEquivalent equivalent,
    ContractorID contractorID,
    const TransactionUUID &transactionUUID,
    const vector<pair<PathID, ConstSharedTrustLineAmount>> &finalAmountsConfig) :

    TransactionMessage(
        equivalent,
        contractorID,
        transactionUUID)
{
    // Convert pair vector to PathReservation vector using mEquivalent
    mFinalAmountsConfiguration.reserve(finalAmountsConfig.size());
    for (const auto& [pathID, amount] : finalAmountsConfig) {
        mFinalAmountsConfiguration.emplace_back(pathID, amount, equivalent);
    }
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

        SerializedEquivalent equivalent;
        deserializer.copyInto(&equivalent);

        mFinalAmountsConfiguration.emplace_back(
            pathID,
            make_shared<const TrustLineAmount>(trustLineAmount),
            equivalent);
    }
}

const vector<PathReservation> &RequestMessageWithReservations::finalAmountsConfiguration() const {
    return mFinalAmountsConfiguration;
}

vector<pair<PathID, ConstSharedTrustLineAmount>> RequestMessageWithReservations::finalAmountsConfigurationDeprecated() const {
    vector<pair<PathID, ConstSharedTrustLineAmount>> result;
    result.reserve(mFinalAmountsConfiguration.size());
    for (const auto& reservation : mFinalAmountsConfiguration) {
        result.emplace_back(reservation.pathID, reservation.amount);
    }
    return result;
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

    for (auto const &reservation : mFinalAmountsConfiguration) {
        serializer.copy(reservation.pathID);
        serializer.copy(*reservation.amount.get());
        serializer.copy(reservation.equivalent);
    }

    return serializer.collect();
}

const size_t RequestMessageWithReservations::kOffsetToInheritedBytes() const
{
    auto kOffset =
        TransactionMessage::kOffsetToInheritedBytes() +
        BytesSerializer::kSerializedRecordsCountSize +
        finalAmountsConfiguration().size() * (
            BytesSerializer::kSerializedPathIDSize +
            BytesSerializer::kSerializedTrustLineAmountSize +
            sizeof(SerializedEquivalent));

    return kOffset;
}
