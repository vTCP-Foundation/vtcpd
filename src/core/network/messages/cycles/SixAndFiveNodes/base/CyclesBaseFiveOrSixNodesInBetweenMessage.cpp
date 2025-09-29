#include "CyclesBaseFiveOrSixNodesInBetweenMessage.h"
#include "../../../../common/serialization/BytesDeserializer.h"
#include "../../../../common/serialization/BytesSerializer.h"

CycleBaseFiveOrSixNodesInBetweenMessage::CycleBaseFiveOrSixNodesInBetweenMessage(
    const SerializedEquivalent equivalent,
    ContractorID idOnReceiverSide,
    vector<BaseAddress::Shared> &path):
    SenderMessage(
        equivalent,
        idOnReceiverSide),
    mPath(path)
{}

CycleBaseFiveOrSixNodesInBetweenMessage::CycleBaseFiveOrSixNodesInBetweenMessage(
    BytesShared buffer):
    SenderMessage(buffer)
{
    auto deserializer = BytesDeserializer(
        buffer,
        SenderMessage::kOffsetToInheritedBytes());

    // path
    SerializedPositionInPath nodesInPath;
    deserializer.copyInto(&nodesInPath);

    size_t currentOffset = SenderMessage::kOffsetToInheritedBytes() + BytesSerializer::kSerializedByteSize;
    for (SerializedPositionInPath idx = 0; idx < nodesInPath; idx++) {
        auto stepAddress = deserializeAddress(
                               buffer.get() + currentOffset);
        currentOffset += stepAddress->serializedSize();
        mPath.push_back(stepAddress);
    }
}

pair<BytesShared, size_t> CycleBaseFiveOrSixNodesInBetweenMessage::serializeToBytes() const
{
    auto serializer = BytesSerializer();

    // Serialize parent data
    serializer.enqueue(SenderMessage::serializeToBytes());

    // Serialize path length
    auto kNodesInPath = (SerializedPathLengthSize)mPath.size();
    serializer.copy(kNodesInPath);

    // Serialize path addresses
    for(auto const &address: mPath) {
        serializer.enqueue(address->serializeToBytes(), address->serializedSize());
    }

    return serializer.collect();
}

const size_t CycleBaseFiveOrSixNodesInBetweenMessage::kOffsetToInheritedBytes() const
{
    auto kNodesInPath = (SerializedPathLengthSize)mPath.size();
    size_t offset = SenderMessage::kOffsetToInheritedBytes()
                    + BytesSerializer::kSerializedByteSize;
    for (const auto &address : mPath) {
        offset += address->serializedSize();
    }
    return offset;
}


vector<BaseAddress::Shared> CycleBaseFiveOrSixNodesInBetweenMessage::path() const
{
    return mPath;
}
