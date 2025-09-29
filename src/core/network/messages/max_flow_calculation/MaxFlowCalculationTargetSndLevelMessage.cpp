#include "MaxFlowCalculationTargetSndLevelMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"

MaxFlowCalculationTargetSndLevelMessage::MaxFlowCalculationTargetSndLevelMessage(
    const SerializedEquivalent equivalent,
    ContractorID idOnReceiverSide,
    vector<BaseAddress::Shared> targetAddresses,
    bool isTargetGateway) : MaxFlowCalculationMessage(equivalent,
                idOnReceiverSide,
                targetAddresses),
    mIsTargetGateway(isTargetGateway)
{
}

MaxFlowCalculationTargetSndLevelMessage::MaxFlowCalculationTargetSndLevelMessage(
    const SerializedEquivalent equivalent,
    ContractorID idOnReceiverSide,
    vector<BaseAddress::Shared> targetAddresses,
    bool isTargetGateway,
    vector<SerializedEquivalent> exchangeEquivalents) : MaxFlowCalculationMessage(equivalent,
                idOnReceiverSide,
                targetAddresses,
                exchangeEquivalents),
    mIsTargetGateway(isTargetGateway)
{
}

MaxFlowCalculationTargetSndLevelMessage::MaxFlowCalculationTargetSndLevelMessage(
    BytesShared buffer) : MaxFlowCalculationMessage(buffer)
{
    size_t bytesBufferOffset = MaxFlowCalculationMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, bytesBufferOffset);

    deserializer.copyInto(&mIsTargetGateway);
}

bool MaxFlowCalculationTargetSndLevelMessage::isTargetGateway() const
{
    return mIsTargetGateway;
}

const Message::MessageType MaxFlowCalculationTargetSndLevelMessage::typeID() const
{
    return Message::MessageType::MaxFlow_CalculationTargetSecondLevel;
}

pair<BytesShared, size_t> MaxFlowCalculationTargetSndLevelMessage::serializeToBytes() const
{
    auto parentBytesAndCount = MaxFlowCalculationMessage::serializeToBytes();
    size_t bytesCount =
        parentBytesAndCount.second +
        sizeof(byte_t);

    // Use BytesSerializer for consistent serialization
    BytesSerializer serializer;
    serializer.enqueue(parentBytesAndCount);
    serializer.copy(mIsTargetGateway);

    return serializer.collect();
}
