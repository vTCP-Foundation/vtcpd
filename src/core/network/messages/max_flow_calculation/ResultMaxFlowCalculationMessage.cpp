#include "ResultMaxFlowCalculationMessage.h"

ResultMaxFlowCalculationMessage::ResultMaxFlowCalculationMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> &outgoingFlows,
    vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> &incomingFlows,
    Commission::Shared commission) :

    MaxFlowCalculationConfirmationMessage(
        equivalent,
        senderAddresses,
        0),
    mOutgoingFlows(outgoingFlows),
    mIncomingFlows(incomingFlows),
    mCommission(commission)
{
}

ResultMaxFlowCalculationMessage::ResultMaxFlowCalculationMessage(
    BytesShared buffer) :

    MaxFlowCalculationConfirmationMessage(buffer)
{
    size_t bytesBufferOffset = MaxFlowCalculationConfirmationMessage::kOffsetToInheritedBytes();
    //----------------------------------------------------
    auto *trustLinesOutCount = new (buffer.get() + bytesBufferOffset) SerializedRecordsCount;
    bytesBufferOffset += sizeof(SerializedRecordsCount);
    //-----------------------------------------------------
    mOutgoingFlows.reserve(*trustLinesOutCount);
    for (SerializedRecordNumber idx = 0; idx < *trustLinesOutCount; idx++) {
        auto address = deserializeAddress(
                           buffer.get() + bytesBufferOffset);
        bytesBufferOffset += address->serializedSize();
        //---------------------------------------------------
        vector<byte_t> bufferTrustLineAmount(
            buffer.get() + bytesBufferOffset,
            buffer.get() + bytesBufferOffset + kTrustLineAmountBytesCount);
        bytesBufferOffset += kTrustLineAmountBytesCount;
        //---------------------------------------------------
        TrustLineAmount trustLineAmount = bytesToTrustLineAmount(bufferTrustLineAmount);
        mOutgoingFlows.emplace_back(
            address,
            make_shared<const TrustLineAmount>(
                trustLineAmount));
    }
    //----------------------------------------------------
    auto *trustLinesInCount = new (buffer.get() + bytesBufferOffset) SerializedRecordsCount;
    bytesBufferOffset += sizeof(SerializedRecordsCount);
    //-----------------------------------------------------
    mIncomingFlows.reserve(*trustLinesInCount);
    for (SerializedRecordNumber idx = 0; idx < *trustLinesInCount; idx++) {
        auto address = deserializeAddress(
                           buffer.get() + bytesBufferOffset);
        bytesBufferOffset += address->serializedSize();
        //---------------------------------------------------
        vector<byte_t> bufferTrustLineAmount(
            buffer.get() + bytesBufferOffset,
            buffer.get() + bytesBufferOffset + kTrustLineAmountBytesCount);
        bytesBufferOffset += kTrustLineAmountBytesCount;
        //---------------------------------------------------
        TrustLineAmount trustLineAmount = bytesToTrustLineAmount(bufferTrustLineAmount);
        mIncomingFlows.emplace_back(
            address,
            make_shared<const TrustLineAmount>(
                trustLineAmount));
    }
    
    // Read commission (always present in protocol)
    auto hasCommission = *(buffer.get() + bytesBufferOffset);
    bytesBufferOffset += sizeof(byte_t);
    
    if (hasCommission) {
        uint64_t commissionAmount;
        memcpy(&commissionAmount, buffer.get() + bytesBufferOffset, sizeof(uint64_t));
        mCommission = make_shared<Commission>(commissionAmount);
        bytesBufferOffset += sizeof(uint64_t);
    }
}

const Message::MessageType ResultMaxFlowCalculationMessage::typeID() const
{
    return Message::MessageType::MaxFlow_ResultMaxFlowCalculation;
}

const bool ResultMaxFlowCalculationMessage::isAddToConfirmationNotStronglyRequiredMessagesHandler() const
{
    return true;
}

pair<BytesShared, size_t> ResultMaxFlowCalculationMessage::serializeToBytes() const
{
    auto parentBytesAndCount = MaxFlowCalculationConfirmationMessage::serializeToBytes();
    size_t bytesCount = parentBytesAndCount.second + sizeof(SerializedRecordsCount) + sizeof(SerializedRecordsCount);
    for (const auto &outgoingFlow : mOutgoingFlows) {
        bytesCount += outgoingFlow.first->serializedSize() + kTrustLineAmountBytesCount;
    }
    for (const auto &incomingFlow : mIncomingFlows) {
        bytesCount += incomingFlow.first->serializedSize() + kTrustLineAmountBytesCount;
    }
    
    // Add bytes for commission (has_commission flag + commission amount)
    bytesCount += sizeof(byte_t);
    if (mCommission) {
        bytesCount += sizeof(uint64_t);
    }
    BytesShared dataBytesShared = tryCalloc(bytesCount);

    size_t dataBytesOffset = 0;
    //----------------------------------------------------
    memcpy(
        dataBytesShared.get(),
        parentBytesAndCount.first.get(),
        parentBytesAndCount.second);
    dataBytesOffset += parentBytesAndCount.second;
    //----------------------------------------------------

    auto trustLinesOutCount = (SerializedRecordsCount)mOutgoingFlows.size();
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &trustLinesOutCount,
        sizeof(SerializedRecordsCount));
    dataBytesOffset += sizeof(SerializedRecordsCount);
    //----------------------------------------------------
    for (auto const &outgoingFlow : mOutgoingFlows) {
        auto serializedData = outgoingFlow.first->serializeToBytes();
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            serializedData.get(),
            outgoingFlow.first->serializedSize());
        dataBytesOffset += outgoingFlow.first->serializedSize();
        //------------------------------------------------
        vector<byte_t> buffer = trustLineAmountToBytes(*outgoingFlow.second.get());
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            buffer.data(),
            buffer.size());
        dataBytesOffset += kTrustLineAmountBytesCount;
    }
    //----------------------------------------------------
    auto trustLinesInCount = (SerializedRecordsCount)mIncomingFlows.size();
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &trustLinesInCount,
        sizeof(SerializedRecordsCount));
    dataBytesOffset += sizeof(SerializedRecordsCount);
    //----------------------------------------------------
    for (auto const &incomingFlow : mIncomingFlows) {
        auto serializedData = incomingFlow.first->serializeToBytes();
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            serializedData.get(),
            incomingFlow.first->serializedSize());
        dataBytesOffset += incomingFlow.first->serializedSize();
        //------------------------------------------------
        vector<byte_t> buffer = trustLineAmountToBytes(*incomingFlow.second.get());
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            buffer.data(),
            buffer.size());
        dataBytesOffset += kTrustLineAmountBytesCount;
    }
    //----------------------------------------------------
    
    // Serialize commission
    byte_t hasCommission = mCommission ? 1 : 0;
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &hasCommission,
        sizeof(byte_t));
    dataBytesOffset += sizeof(byte_t);
    
    if (mCommission) {
        uint64_t commissionAmount = mCommission->amount();
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            &commissionAmount,
            sizeof(uint64_t));
        dataBytesOffset += sizeof(uint64_t);
    }
    
    //----------------------------------------------------
    return make_pair(
               dataBytesShared,
               bytesCount);
}

const vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> ResultMaxFlowCalculationMessage::outgoingFlows() const {
    return mOutgoingFlows;
}

const vector<pair<BaseAddress::Shared, ConstSharedTrustLineAmount>> ResultMaxFlowCalculationMessage::incomingFlows() const
{
    return mIncomingFlows;
}

Commission::Shared ResultMaxFlowCalculationMessage::commission() const
{
    return mCommission;
}
