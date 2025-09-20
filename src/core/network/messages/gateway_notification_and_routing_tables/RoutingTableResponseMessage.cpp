#include "RoutingTableResponseMessage.h"
#include "../../common/serialization/BytesDeserializer.h"
#include "../../common/serialization/BytesSerializer.h"

RoutingTableResponseMessage::RoutingTableResponseMessage(
    ContractorID idOnReceiverSide,
    const TransactionUUID &transactionUUID,
    vector<pair<SerializedEquivalent, vector<BaseAddress::Shared>>> neighbors):
    ConfirmationMessage(
        0,
        idOnReceiverSide,
        transactionUUID),
    mNeighbors(neighbors)
{}

RoutingTableResponseMessage::RoutingTableResponseMessage(
    BytesShared buffer):
    ConfirmationMessage(buffer)
{
    auto deserializer = BytesDeserializer(
        buffer,
        ConfirmationMessage::kOffsetToInheritedBytes());

    SerializedRecordsCount equivalentsNumber;
    deserializer.copyInto(&equivalentsNumber);
    mNeighbors.reserve(equivalentsNumber);

    size_t currentOffset = ConfirmationMessage::kOffsetToInheritedBytes() + BytesSerializer::kSerializedRecordsCountSize;
    for (SerializedRecordNumber idxEq = 0; idxEq < equivalentsNumber; idxEq++) {
        auto equivalentDeserializer = BytesDeserializer(
            buffer,
            currentOffset);

        SerializedEquivalent equivalentTmp;
        equivalentDeserializer.copyInto(&equivalentTmp);
        currentOffset += BytesSerializer::kSerializedEquivalentSize;

        SerializedRecordsCount neighborsNumber;
        equivalentDeserializer = BytesDeserializer(
            buffer,
            currentOffset);
        equivalentDeserializer.copyInto(&neighborsNumber);
        currentOffset += BytesSerializer::kSerializedRecordsCountSize;

        vector<BaseAddress::Shared> neighborsAddresses;
        neighborsAddresses.reserve(neighborsNumber);
        for (SerializedRecordNumber idx = 0; idx < neighborsNumber; idx++) {
            auto address = deserializeAddress(
                               buffer.get() + currentOffset);
            currentOffset += address->serializedSize();
            neighborsAddresses.push_back(address);
        }
        mNeighbors.emplace_back(
            equivalentTmp,
            neighborsAddresses);
    }
}

const Message::MessageType RoutingTableResponseMessage::typeID() const {
    return Message::MessageType::RoutingTableResponse;
}

pair<BytesShared, size_t> RoutingTableResponseMessage::serializeToBytes() const
{
    auto serializer = BytesSerializer();

    // Serialize parent data
    serializer.enqueue(ConfirmationMessage::serializeToBytes());

    // Serialize equivalents count
    auto equivalentsCount = (SerializedRecordsCount)mNeighbors.size();
    serializer.copy(equivalentsCount);

    // Serialize equivalents and their neighbors
    for (const auto &equivalentAndNeighbors : mNeighbors) {
        // Serialize equivalent
        serializer.copy(equivalentAndNeighbors.first);

        // Serialize neighbors count
        auto neighborsCount = (SerializedRecordsCount)equivalentAndNeighbors.second.size();
        serializer.copy(neighborsCount);

        // Serialize neighbor addresses
        for (const auto &neighborAddress: equivalentAndNeighbors.second) {
            serializer.enqueue(neighborAddress->serializeToBytes(), neighborAddress->serializedSize());
        }
    }

    return serializer.collect();
}

vector<pair<SerializedEquivalent, vector<BaseAddress::Shared>>> RoutingTableResponseMessage::neighborsByEquivalents() const {
    return mNeighbors;
}
