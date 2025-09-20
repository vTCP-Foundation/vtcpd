#include "MaxFlowCalculationSourceFstLevelMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"


MaxFlowCalculationSourceFstLevelMessage::MaxFlowCalculationSourceFstLevelMessage(
	const SerializedEquivalent equivalent,
	ContractorID idOnReceiverSide,
	HopsCount_t hopsCount) :
	SenderMessage(
        equivalent,
        idOnReceiverSide),
	mHopsCnt(hopsCount)
{}

const Message::MessageType
 MaxFlowCalculationSourceFstLevelMessage::typeID() const {
    return Message::MaxFlow_CalculationSourceFirstLevel;
}

HopsCount_t 
MaxFlowCalculationSourceFstLevelMessage::getHopsCount() const {
	return this->mHopsCnt;
}


MaxFlowCalculationSourceFstLevelMessage::MaxFlowCalculationSourceFstLevelMessage(
	BytesShared buffer) : SenderMessage(buffer) {
	
	size_t bytesBufferOffset = SenderMessage::kOffsetToInheritedBytes();
	BytesDeserializer deserializer(buffer, bytesBufferOffset);

	deserializer.copyInto(&mHopsCnt);
}

pair<BytesShared, size_t>
MaxFlowCalculationSourceFstLevelMessage::serializeToBytes() const {
	
	auto parentBytesAndCount = SenderMessage::serializeToBytes();
    size_t bytesCount =
            parentBytesAndCount.second +
            sizeof(HopsCount_t);

    // Use BytesSerializer for consistent serialization
    BytesSerializer serializer;
    serializer.enqueue(parentBytesAndCount);
    serializer.copy(mHopsCnt);

    return serializer.collect();
}
