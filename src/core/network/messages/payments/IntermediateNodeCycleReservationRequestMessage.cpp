#include "IntermediateNodeCycleReservationRequestMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"

IntermediateNodeCycleReservationRequestMessage::IntermediateNodeCycleReservationRequestMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID& transactionUUID,
    const TrustLineAmount& amount,
    BaseAddress::Shared coordinatorAddress,
    SerializedPathLengthSize cycleLength) :

    RequestCycleMessage(
        equivalent,
        senderAddresses,
        transactionUUID,
        amount),
    mCoordinatorAddress(coordinatorAddress),
    mCycleLength(cycleLength)
{}

IntermediateNodeCycleReservationRequestMessage::IntermediateNodeCycleReservationRequestMessage(
    BytesShared buffer) :

    RequestCycleMessage(buffer)
{
    auto currentOffset = RequestCycleMessage::kOffsetToInheritedBytes();
    auto dataBytesOffset = buffer.get() + currentOffset;

    mCoordinatorAddress = deserializeAddress(dataBytesOffset);
    dataBytesOffset += mCoordinatorAddress->serializedSize();

    BytesDeserializer deserializer(buffer, dataBytesOffset - buffer.get());
    deserializer.copyIntoDespiteConst(&mCycleLength);
}

const Message::MessageType IntermediateNodeCycleReservationRequestMessage::typeID() const
{
    return Message::Payments_IntermediateNodeCycleReservationRequest;
}

SerializedPathLengthSize IntermediateNodeCycleReservationRequestMessage::cycleLength() const
{
    return mCycleLength;
}

BaseAddress::Shared IntermediateNodeCycleReservationRequestMessage::coordinatorAddress() const
{
    return mCoordinatorAddress;
}

pair<BytesShared, size_t> IntermediateNodeCycleReservationRequestMessage::serializeToBytes() const
{
    BytesSerializer serializer;

    serializer.enqueue(RequestCycleMessage::serializeToBytes());

    auto serializedAddress = mCoordinatorAddress->serializeToBytes();
    serializer.enqueue(
        serializedAddress,
        mCoordinatorAddress->serializedSize());

    serializer.copy(mCycleLength);

    return serializer.collect();
}