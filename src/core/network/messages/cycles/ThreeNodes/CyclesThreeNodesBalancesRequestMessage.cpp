#include "CyclesThreeNodesBalancesRequestMessage.h"
#include "../../../../common/serialization/BytesDeserializer.h"


CyclesThreeNodesBalancesRequestMessage::CyclesThreeNodesBalancesRequestMessage(
    const SerializedEquivalent equivalent,
    ContractorID idOnReceiverSide,
    const TransactionUUID &transactionUUID,
    vector<BaseAddress::Shared> &neighbors):

    TransactionMessage(
        equivalent,
        idOnReceiverSide,
        transactionUUID),
    mNeighbors(neighbors)
{}

CyclesThreeNodesBalancesRequestMessage::CyclesThreeNodesBalancesRequestMessage(
    BytesShared buffer):

    TransactionMessage(buffer)
{
    auto currentOffset = TransactionMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, currentOffset);

    SerializedRecordsCount neighborsCount;
    deserializer.copyInto(&neighborsCount);
    currentOffset += BytesSerializer::kSerializedRecordsCountSize;

    for (SerializedRecordNumber idx = 0; idx < neighborsCount; idx++) {
        auto stepAddress = deserializeAddress(buffer.get() + currentOffset);
        currentOffset += stepAddress->serializedSize();
        mNeighbors.push_back(stepAddress);
    }
}

pair<BytesShared, size_t> CyclesThreeNodesBalancesRequestMessage::serializeToBytes() const
{
    BytesSerializer serializer;

    serializer.enqueue(TransactionMessage::serializeToBytes());

    auto neighborsCount = (SerializedRecordsCount)mNeighbors.size();
    serializer.copy(neighborsCount);

    for(auto const &address: mNeighbors) {
        auto serializedData = address->serializeToBytes();
        serializer.enqueue(
            serializedData,
            address->serializedSize());
    }

    return serializer.collect();
}

const Message::MessageType CyclesThreeNodesBalancesRequestMessage::typeID() const
{
    return Message::MessageType::Cycles_ThreeNodesBalancesRequest;
}

vector<BaseAddress::Shared> CyclesThreeNodesBalancesRequestMessage::neighbors()
{
    return mNeighbors;
}
