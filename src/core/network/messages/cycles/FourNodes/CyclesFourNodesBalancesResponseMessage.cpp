#include "CyclesFourNodesBalancesResponseMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"
#include "../../../common/serialization/BytesSerializer.h"

CyclesFourNodesBalancesResponseMessage::CyclesFourNodesBalancesResponseMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID &transactionUUID,
    vector<BaseAddress::Shared> &suitableNodes):

    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID),
    mSuitableNodes(suitableNodes)
{}

CyclesFourNodesBalancesResponseMessage::CyclesFourNodesBalancesResponseMessage(
    BytesShared buffer):

    TransactionMessage(buffer)
{
    auto deserializer = BytesDeserializer(
        buffer,
        TransactionMessage::kOffsetToInheritedBytes());

    SerializedRecordsCount suitableNodesCount;
    deserializer.copyInto(&suitableNodesCount);

    size_t currentOffset = TransactionMessage::kOffsetToInheritedBytes() + BytesSerializer::kSerializedRecordsCountSize;
    for (SerializedRecordNumber idx = 0; idx < suitableNodesCount; idx++) {
        auto stepAddress = deserializeAddress(
                               buffer.get() + currentOffset);
        currentOffset += stepAddress->serializedSize();
        mSuitableNodes.push_back(stepAddress);
    }
}

pair<BytesShared, size_t> CyclesFourNodesBalancesResponseMessage::serializeToBytes() const
{
    auto serializer = BytesSerializer();

    // Serialize parent data
    serializer.enqueue(TransactionMessage::serializeToBytes());

    // Serialize suitable nodes count
    auto contractorsCount = (SerializedRecordsCount)mSuitableNodes.size();
    serializer.copy(contractorsCount);

    // Serialize suitable nodes
    for(auto const &address: mSuitableNodes) {
        serializer.enqueue(address->serializeToBytes(), address->serializedSize());
    }

    return serializer.collect();
}

vector<BaseAddress::Shared> CyclesFourNodesBalancesResponseMessage::suitableNodes() const
{
    return mSuitableNodes;
}

const Message::MessageType CyclesFourNodesBalancesResponseMessage::typeID() const
{
    return Message::MessageType::Cycles_FourNodesBalancesResponse;
}