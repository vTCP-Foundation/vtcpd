#include "CyclesThreeNodesBalancesResponseMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"
#include "../../../common/serialization/BytesSerializer.h"


CyclesThreeNodesBalancesResponseMessage::CyclesThreeNodesBalancesResponseMessage(
    const SerializedEquivalent equivalent,
    ContractorID idOnReceiverSide,
    const TransactionUUID &transactionUUID,
    vector<BaseAddress::Shared> &neighbors) :
    TransactionMessage(
        equivalent,
        idOnReceiverSide,
        transactionUUID),
    mNeighbors(neighbors)
{}

CyclesThreeNodesBalancesResponseMessage::CyclesThreeNodesBalancesResponseMessage(
    BytesShared buffer):

    TransactionMessage(buffer)
{
    auto deserializer = BytesDeserializer(
        buffer,
        TransactionMessage::kOffsetToInheritedBytes());

    //    Get NodesCount
    SerializedRecordsCount boundaryNodesCount;
    deserializer.copyInto(&boundaryNodesCount);

    //    Parse boundary nodes
    size_t currentOffset = TransactionMessage::kOffsetToInheritedBytes() + BytesSerializer::kSerializedRecordsCountSize;
    for (SerializedRecordNumber idx = 0; idx < boundaryNodesCount; idx++) {
        auto stepAddress = deserializeAddress(
                               buffer.get() + currentOffset);
        currentOffset += stepAddress->serializedSize();
        mNeighbors.push_back(stepAddress);
    }
}

std::pair<BytesShared, size_t> CyclesThreeNodesBalancesResponseMessage::serializeToBytes() const
{
    auto serializer = BytesSerializer();

    // Serialize parent data
    serializer.enqueue(TransactionMessage::serializeToBytes());

    // Serialize neighbors count
    auto boundaryNodesCount = (SerializedRecordsCount) mNeighbors.size();
    serializer.copy(boundaryNodesCount);

    // Serialize neighbors
    for(const auto &kNodeAddress: mNeighbors) {
        serializer.enqueue(kNodeAddress->serializeToBytes(), kNodeAddress->serializedSize());
    }

    return serializer.collect();
}

const Message::MessageType CyclesThreeNodesBalancesResponseMessage::typeID() const
{
    return Message::MessageType::Cycles_ThreeNodesBalancesResponse;
}

vector<BaseAddress::Shared> CyclesThreeNodesBalancesResponseMessage::commonNodes()
{
    return mNeighbors;
}
