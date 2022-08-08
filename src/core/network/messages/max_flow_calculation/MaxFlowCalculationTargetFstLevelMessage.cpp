#include "MaxFlowCalculationTargetFstLevelMessage.h"

MaxFlowCalculationTargetFstLevelMessage::MaxFlowCalculationTargetFstLevelMessage(
    const SerializedEquivalent equivalent,
    ContractorID idOnReceiverSide,
    vector<BaseAddress::Shared> targetAddresses,
    bool isTargetGateway,
	uint8_t hopsCount):
    MaxFlowCalculationMessage(
        equivalent,
        idOnReceiverSide,
        targetAddresses),
    mIsTargetGateway(isTargetGateway),
	mHopsCnt(hopsCount)
{}

MaxFlowCalculationTargetFstLevelMessage::MaxFlowCalculationTargetFstLevelMessage(
    BytesShared buffer):
    MaxFlowCalculationMessage(buffer)
{
    size_t bytesBufferOffset = MaxFlowCalculationMessage::kOffsetToInheritedBytes();

    memcpy(
        &mIsTargetGateway,
        buffer.get() + bytesBufferOffset,
        sizeof(byte));
	
	bytesBufferOffset += sizeof(byte);
	
	memcpy(
		&mHopsCnt,
		buffer.get() + bytesBufferOffset,
		sizeof(byte));
}

bool MaxFlowCalculationTargetFstLevelMessage::isTargetGateway() const
{
    return mIsTargetGateway;
}

const Message::MessageType MaxFlowCalculationTargetFstLevelMessage::typeID() const
{
    return Message::MessageType::MaxFlow_CalculationTargetFirstLevel;
}

pair<BytesShared, size_t> MaxFlowCalculationTargetFstLevelMessage::serializeToBytes() const
{
    auto parentBytesAndCount = MaxFlowCalculationMessage::serializeToBytes();
    size_t bytesCount =
        parentBytesAndCount.second + 
		sizeof(byte) +
        sizeof(byte); // 1 byte for mIstargetGateway and 1 byte for mHopsCnt;

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
        &mIsTargetGateway,
        sizeof(byte));

	dataBytesOffset += sizeof(byte);

	memcpy(
		dataBytesShared.get() + dataBytesOffset,
		&mHopsCnt, sizeof(byte));

    return make_pair(
        dataBytesShared,
        bytesCount);
}

uint8_t MaxFlowCalculationTargetFstLevelMessage::getHopsCount() const {
	return this->mHopsCnt;
}
