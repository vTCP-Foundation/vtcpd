#include "ResultMaxFlowCalculationMessage.h"
#include "../../common/serialization/BytesDeserializer.h"
#include "../../common/serialization/BytesSerializer.h"

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
    auto deserializer = BytesDeserializer(
        buffer,
        MaxFlowCalculationConfirmationMessage::kOffsetToInheritedBytes());

    //----------------------------------------------------
    SerializedRecordsCount trustLinesOutCount;
    deserializer.copyInto(&trustLinesOutCount);

    //-----------------------------------------------------
    mOutgoingFlows.reserve(trustLinesOutCount);
    size_t currentOffset = MaxFlowCalculationConfirmationMessage::kOffsetToInheritedBytes() + BytesSerializer::kSerializedRecordsCountSize;
    for (SerializedRecordNumber idx = 0; idx < trustLinesOutCount; idx++) {
        auto address = deserializeAddress(
                           buffer.get() + currentOffset);
        currentOffset += address->serializedSize();
        //---------------------------------------------------
        auto amountDeserializer = BytesDeserializer(
            buffer,
            currentOffset);
        TrustLineAmount trustLineAmount;
        amountDeserializer.copyInto(&trustLineAmount);
        currentOffset += BytesSerializer::kSerializedTrustLineAmountSize;
        //---------------------------------------------------
        mOutgoingFlows.emplace_back(
            address,
            make_shared<const TrustLineAmount>(
                trustLineAmount));
    }
    //----------------------------------------------------
    auto secondDeserializer = BytesDeserializer(
        buffer,
        currentOffset);
    SerializedRecordsCount trustLinesInCount;
    secondDeserializer.copyInto(&trustLinesInCount);
    currentOffset += BytesSerializer::kSerializedRecordsCountSize;

    //-----------------------------------------------------
    mIncomingFlows.reserve(trustLinesInCount);
    for (SerializedRecordNumber idx = 0; idx < trustLinesInCount; idx++) {
        auto address = deserializeAddress(
                           buffer.get() + currentOffset);
        currentOffset += address->serializedSize();
        //---------------------------------------------------
        auto amountDeserializer = BytesDeserializer(
            buffer,
            currentOffset);
        TrustLineAmount trustLineAmount;
        amountDeserializer.copyInto(&trustLineAmount);
        currentOffset += BytesSerializer::kSerializedTrustLineAmountSize;
        //---------------------------------------------------
        mIncomingFlows.emplace_back(
            address,
            make_shared<const TrustLineAmount>(
                trustLineAmount));
    }

    // Read commission presence flag and optional commission amount
    auto hasCommissionDeserializer = BytesDeserializer(
        buffer,
        currentOffset);
    byte_t hasCommission = 0;
    hasCommissionDeserializer.copyInto(&hasCommission);
    currentOffset += BytesSerializer::kSerializedByteSize;

    if (hasCommission) {
        BytesDeserializer commissionDeserializer(
            buffer,
            currentOffset);
        uint64_t commissionAmount;
        commissionDeserializer.copyInto(&commissionAmount);
        mCommission = make_shared<Commission>(commissionAmount);
        currentOffset += BytesSerializer::kSerializedUInt64Size;
    } else {
        mCommission = nullptr;
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
    auto serializer = BytesSerializer();

    // Serialize parent data
    serializer.enqueue(MaxFlowCalculationConfirmationMessage::serializeToBytes());

    // Serialize outgoing flows count
    auto trustLinesOutCount = (SerializedRecordsCount)mOutgoingFlows.size();
    serializer.copy(trustLinesOutCount);

    // Serialize outgoing flows
    for (auto const &outgoingFlow : mOutgoingFlows) {
        serializer.enqueue(outgoingFlow.first->serializeToBytes(), outgoingFlow.first->serializedSize());
        serializer.copy(*outgoingFlow.second.get());
    }

    // Serialize incoming flows count
    auto trustLinesInCount = (SerializedRecordsCount)mIncomingFlows.size();
    serializer.copy(trustLinesInCount);

    // Serialize incoming flows
    for (auto const &incomingFlow : mIncomingFlows) {
        serializer.enqueue(incomingFlow.first->serializeToBytes(), incomingFlow.first->serializedSize());
        serializer.copy(*incomingFlow.second.get());
    }

    // Serialize commission presence flag and optional commission amount
    byte_t hasCommission = mCommission ? 1 : 0;
    serializer.copy(hasCommission);
    if (mCommission) {
        uint64_t commissionAmount = mCommission->amount();
        serializer.copy(commissionAmount);
    }

    return serializer.collect();
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
