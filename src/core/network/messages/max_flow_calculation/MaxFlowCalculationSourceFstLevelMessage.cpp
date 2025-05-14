#include "MaxFlowCalculationSourceFstLevelMessage.h"


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
	    
	memcpy(
        &mHopsCnt,
        buffer.get() + bytesBufferOffset,
        sizeof(HopsCount_t));
}

pair<BytesShared, size_t>
MaxFlowCalculationSourceFstLevelMessage::serializeToBytes() const {
	
	auto parentBytesAndCount = SenderMessage::serializeToBytes();
    size_t bytesCount =
            parentBytesAndCount.second +
            sizeof(HopsCount_t);

    BytesShared dataBytesShared = tryCalloc(bytesCount);
    size_t dataBytesOffset = 0;
    //----------------------------------------------------
    memcpy(
        dataBytesShared.get(),
        parentBytesAndCount.first.get(),
        parentBytesAndCount.second);
    dataBytesOffset += parentBytesAndCount.second;

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mHopsCnt,
        sizeof(HopsCount_t));

    return make_pair(
        dataBytesShared,
        bytesCount);
}
