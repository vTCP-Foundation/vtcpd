#include "MaxFlowCalculationSourceFstLevelMessage.h"


MaxFlowCalculationSourceFstLevelMessage::MaxFlowCalculationSourceFstLevelMessage(
	const SerializedEquivalent equivalent,
	ContractorID idOnReceiverSide,
	bool isSenderGateway, 
	uint8_t hopsCount) :
	SenderMessage(
        equivalent,
        idOnReceiverSide),
    mIsSenderGateway(isSenderGateway),
	mHopsCnt(hopsCount)
{}

const Message::MessageType
 MaxFlowCalculationSourceFstLevelMessage::typeID() const {
    return Message::MaxFlow_CalculationSourceFirstLevel;
}

uint8_t 
MaxFlowCalculationSourceFstLevelMessage::getHopsCount() const {
	return this->mHopsCnt;
}


MaxFlowCalculationSourceFstLevelMessage::MaxFlowCalculationSourceFstLevelMessage(
	BytesShared buffer) : SenderMessage(buffer) {
	
	 size_t bytesBufferOffset = SenderMessage::kOffsetToInheritedBytes();
	
	memcpy(
        &mIsSenderGateway,
        buffer.get() + bytesBufferOffset,
        sizeof(byte));

    bytesBufferOffset += sizeof(byte);
    
	memcpy(
        &mHopsCnt,
        buffer.get() + bytesBufferOffset,
        sizeof(uint8_t));
}

pair<BytesShared, size_t>
MaxFlowCalculationSourceFstLevelMessage::serializeToBytes() const {
	
	auto parentBytesAndCount = SenderMessage::serializeToBytes();
    size_t bytesCount =
            parentBytesAndCount.second +
            sizeof(byte) +
            sizeof(uint8_t);

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
        &mIsSenderGateway,
        sizeof(byte));
    dataBytesOffset += sizeof(byte);
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mHopsCnt,
        sizeof(uint8_t));

    return make_pair(
        dataBytesShared,
        bytesCount);
}
