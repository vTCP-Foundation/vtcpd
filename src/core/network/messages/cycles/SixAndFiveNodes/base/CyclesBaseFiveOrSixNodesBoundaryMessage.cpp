#include "CyclesBaseFiveOrSixNodesBoundaryMessage.h"
#include "../../../../common/serialization/BytesDeserializer.h"
#include "../../../../common/serialization/BytesSerializer.h"

CyclesBaseFiveOrSixNodesBoundaryMessage::CyclesBaseFiveOrSixNodesBoundaryMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared>& path,
    vector<BaseAddress::Shared>& boundaryNodes) :

    CycleBaseFiveOrSixNodesInBetweenMessage(
        equivalent,
        // todo : this parameter is not useful, need message hierarchy changing
        0,
        path),
    mBoundaryNodes(boundaryNodes)
{}

CyclesBaseFiveOrSixNodesBoundaryMessage::CyclesBaseFiveOrSixNodesBoundaryMessage(
    BytesShared buffer):
    CycleBaseFiveOrSixNodesInBetweenMessage(
        buffer)
{
    auto deserializer = BytesDeserializer(
        buffer,
        CycleBaseFiveOrSixNodesInBetweenMessage::kOffsetToInheritedBytes());

    //    Get NodesCount
    SerializedRecordsCount boundaryNodesCount;
    deserializer.copyInto(&boundaryNodesCount);

    //    Parse boundary nodes
    size_t currentOffset = CycleBaseFiveOrSixNodesInBetweenMessage::kOffsetToInheritedBytes() + BytesSerializer::kSerializedRecordsCountSize;
    for (SerializedRecordNumber idx = 0; idx < boundaryNodesCount; idx++) {
        auto stepAddress = deserializeAddress(
                               buffer.get() + currentOffset);
        currentOffset += stepAddress->serializedSize();
        mBoundaryNodes.push_back(stepAddress);
    }
}

pair<BytesShared, size_t> CyclesBaseFiveOrSixNodesBoundaryMessage::serializeToBytes() const
{
    auto serializer = BytesSerializer();

    // Serialize parent data
    serializer.enqueue(CycleBaseFiveOrSixNodesInBetweenMessage::serializeToBytes());

    // Serialize boundary nodes count
    auto boundaryNodesCount = (SerializedRecordsCount) mBoundaryNodes.size();
    serializer.copy(boundaryNodesCount);

    // Serialize boundary nodes
    for(const auto &address: mBoundaryNodes) {
        serializer.enqueue(address->serializeToBytes(), address->serializedSize());
    }

    return serializer.collect();
}

vector<BaseAddress::Shared> CyclesBaseFiveOrSixNodesBoundaryMessage::boundaryNodes() const
{
    return mBoundaryNodes;
}