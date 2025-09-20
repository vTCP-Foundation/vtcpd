#include "CyclesFourNodesNegativeBalanceRequestMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"
#include "../../../common/serialization/BytesSerializer.h"

CyclesFourNodesNegativeBalanceRequestMessage::CyclesFourNodesNegativeBalanceRequestMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID &transactionUUID,
    BaseAddress::Shared contractorAddress,
    vector<BaseAddress::Shared> checkedNodes):

    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID),
    mContractorAddress(contractorAddress),
    mCheckedNodes(checkedNodes)
{}

CyclesFourNodesNegativeBalanceRequestMessage::CyclesFourNodesNegativeBalanceRequestMessage(
    BytesShared buffer):

    TransactionMessage(buffer)
{
    size_t currentOffset = TransactionMessage::kOffsetToInheritedBytes();

    // contractorAddress
    mContractorAddress = deserializeAddress(
                             buffer.get() + currentOffset);
    currentOffset += mContractorAddress->serializedSize();

    // checkedNodes
    auto deserializer = BytesDeserializer(
        buffer,
        currentOffset);

    SerializedRecordsCount checkedNodesCount;
    deserializer.copyInto(&checkedNodesCount);
    currentOffset += BytesSerializer::kSerializedRecordsCountSize;

    for (SerializedRecordNumber i = 1; i <= checkedNodesCount; ++i) {
        auto stepAddress = deserializeAddress(
                               buffer.get() + currentOffset);
        currentOffset += stepAddress->serializedSize();
        mCheckedNodes.push_back(stepAddress);
    }
}

pair<BytesShared, size_t> CyclesFourNodesNegativeBalanceRequestMessage::serializeToBytes() const
{
    auto serializer = BytesSerializer();

    // Serialize parent data
    serializer.enqueue(TransactionMessage::serializeToBytes());

    // Serialize contractor address
    serializer.enqueue(mContractorAddress->serializeToBytes(), mContractorAddress->serializedSize());

    // Serialize checked nodes count
    auto debtorsCount = (SerializedRecordsCount)mCheckedNodes.size();
    serializer.copy(debtorsCount);

    // Serialize checked nodes
    for(auto const &address: mCheckedNodes) {
        serializer.enqueue(address->serializeToBytes(), address->serializedSize());
    }

    return serializer.collect();
}

const Message::MessageType CyclesFourNodesNegativeBalanceRequestMessage::typeID() const
{
    return Message::MessageType::Cycles_FourNodesNegativeBalanceRequest;
}

vector<BaseAddress::Shared> CyclesFourNodesNegativeBalanceRequestMessage::checkedNodes() const
{
    return mCheckedNodes;
}

BaseAddress::Shared CyclesFourNodesNegativeBalanceRequestMessage::contractorAddress() const
{
    return mContractorAddress;
}