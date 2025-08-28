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
	    
	memcpy(
        &mHopsCnt,
        buffer.get() + bytesBufferOffset,
        sizeof(HopsCount_t));
    bytesBufferOffset += sizeof(HopsCount_t);

    uint8_t exchangeEquivalentsCnt;
    memcpy(
        &exchangeEquivalentsCnt,
        buffer.get() + bytesBufferOffset,
        sizeof(uint8_t));
    bytesBufferOffset += sizeof(uint8_t);

    for (uint8_t idx = 0; idx < exchangeEquivalentsCnt; idx++) {
        SerializedEquivalent equivalent;
        memcpy(
            &equivalent,
            buffer.get() + bytesBufferOffset,
            sizeof(SerializedEquivalent));
        mExchangeEquivalents.push_back(equivalent);
        bytesBufferOffset += sizeof(SerializedEquivalent);
    }
}

pair<BytesShared, size_t>
MaxFlowCalculationSourceFstLevelMessage::serializeToBytes() const {
	
	auto parentBytesAndCount = SenderMessage::serializeToBytes();
    size_t bytesCount =
            parentBytesAndCount.second +
            sizeof(HopsCount_t) +
            sizeof(uint8_t) + // count of exchange equivalents
            mExchangeEquivalents.size() * sizeof(SerializedEquivalent);

    BytesShared dataBytesShared = tryCalloc(bytesCount);
    size_t dataBytesOffset = 0;

    memcpy(
        dataBytesShared.get(),
        parentBytesAndCount.first.get(),
        parentBytesAndCount.second);
    dataBytesOffset += parentBytesAndCount.second;

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mHopsCnt,
        sizeof(HopsCount_t));
    dataBytesOffset += sizeof(HopsCount_t);

    auto exchangeEquivalentsCnt = (uint8_t)mExchangeEquivalents.size();
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &exchangeEquivalentsCnt,
        sizeof(uint8_t));
    dataBytesOffset += sizeof(uint8_t);

    for (const auto &equivalent : mExchangeEquivalents) {
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            &equivalent,
            sizeof(SerializedEquivalent));
        dataBytesOffset += sizeof(SerializedEquivalent);
    }

    return make_pair(
        dataBytesShared,
        bytesCount);
}
