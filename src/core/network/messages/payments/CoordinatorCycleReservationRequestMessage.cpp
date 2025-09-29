#include "CoordinatorCycleReservationRequestMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"

CoordinatorCycleReservationRequestMessage::CoordinatorCycleReservationRequestMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID& transactionUUID,
    const TrustLineAmount& amount,
    BaseAddress::Shared nextNodeInThePathAddress) :

    RequestCycleMessage(
        equivalent,
        senderAddresses,
        transactionUUID,
        amount),
    mNextPathNodeAddress(nextNodeInThePathAddress)
{}

CoordinatorCycleReservationRequestMessage::CoordinatorCycleReservationRequestMessage(
    BytesShared buffer) :

    RequestCycleMessage(buffer)
{
    auto bytesBufferOffset = buffer.get() + RequestCycleMessage::kOffsetToInheritedBytes();

    mNextPathNodeAddress = deserializeAddress(bytesBufferOffset);
}

BaseAddress::Shared CoordinatorCycleReservationRequestMessage::nextNodeInPathAddress() const
{
    return mNextPathNodeAddress;
}

const Message::MessageType CoordinatorCycleReservationRequestMessage::typeID() const
{
    return Message::Payments_CoordinatorCycleReservationRequest;
}

pair<BytesShared, size_t> CoordinatorCycleReservationRequestMessage::serializeToBytes() const
{
    BytesSerializer serializer;

    serializer.enqueue(RequestCycleMessage::serializeToBytes());

    auto serializedAddress = mNextPathNodeAddress->serializeToBytes();
    serializer.enqueue(
        serializedAddress,
        mNextPathNodeAddress->serializedSize());

    return serializer.collect();
}
