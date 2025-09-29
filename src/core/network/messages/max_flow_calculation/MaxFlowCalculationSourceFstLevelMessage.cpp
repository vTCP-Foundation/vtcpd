#include "MaxFlowCalculationSourceFstLevelMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"


MaxFlowCalculationSourceFstLevelMessage::MaxFlowCalculationSourceFstLevelMessage(
	const SerializedEquivalent equivalent,
	ContractorID idOnReceiverSide,
	HopsCount_t hopsCount) :
	SenderMessage(
        equivalent,
        idOnReceiverSide),
	mHopsCnt(hopsCount),
	mExchangeEquivalents()
{}

MaxFlowCalculationSourceFstLevelMessage::MaxFlowCalculationSourceFstLevelMessage(
	const SerializedEquivalent equivalent,
	ContractorID idOnReceiverSide,
	HopsCount_t hopsCount,
	vector<SerializedEquivalent> exchangeEquivalents) :
	SenderMessage(
        equivalent,
        idOnReceiverSide),
	mHopsCnt(hopsCount),
	mExchangeEquivalents(exchangeEquivalents)
{}

const Message::MessageType
 MaxFlowCalculationSourceFstLevelMessage::typeID() const {
    return Message::MaxFlow_CalculationSourceFirstLevel;
}

HopsCount_t 
MaxFlowCalculationSourceFstLevelMessage::getHopsCount() const {
	return this->mHopsCnt;
}

vector<SerializedEquivalent>
MaxFlowCalculationSourceFstLevelMessage::exchangeEquivalents() const {
	return mExchangeEquivalents;
}


MaxFlowCalculationSourceFstLevelMessage::MaxFlowCalculationSourceFstLevelMessage(
	BytesShared buffer) : SenderMessage(buffer) {
	
	size_t bytesBufferOffset = SenderMessage::kOffsetToInheritedBytes();
	BytesDeserializer deserializer(buffer, bytesBufferOffset);

	deserializer.copyInto(&mHopsCnt);

	// Deserialize exchange equivalents count and values sequentially with the same deserializer
	uint8_t exchangeEquivalentsCnt = 0;
	deserializer.copyInto(&exchangeEquivalentsCnt);
	mExchangeEquivalents.clear();
	mExchangeEquivalents.reserve(exchangeEquivalentsCnt);
	for (uint8_t idx = 0; idx < exchangeEquivalentsCnt; idx++) {
		SerializedEquivalent equivalent;
		deserializer.copyInto(&equivalent);
		mExchangeEquivalents.push_back(equivalent);
	}
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


	// Serialize exchange equivalents count and values via BytesSerializer
	auto exchangeEquivalentsCnt = (uint8_t)mExchangeEquivalents.size();
	serializer.copy(exchangeEquivalentsCnt);
	for (const auto &equivalent : mExchangeEquivalents) {
		serializer.copy(equivalent);
	}

    return serializer.collect();
}
