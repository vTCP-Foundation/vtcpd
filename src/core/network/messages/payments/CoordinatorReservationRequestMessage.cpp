#include "CoordinatorReservationRequestMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"


// NEW: Constructor with equivalents
CoordinatorReservationRequestMessage::CoordinatorReservationRequestMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID& transactionUUID,
    const vector<PathReservation> &finalAmountsConfig,
    BaseAddress::Shared nextNodeInThePath) :

    RequestMessageWithReservations(
        equivalent,
        senderAddresses,
        transactionUUID,
        finalAmountsConfig),
    mNextPathNode(nextNodeInThePath)
{}

// DEPRECATED: Constructor without equivalents
CoordinatorReservationRequestMessage::CoordinatorReservationRequestMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID& transactionUUID,
    const vector<pair<PathID, ConstSharedTrustLineAmount>> &finalAmountsConfig,
    BaseAddress::Shared nextNodeInThePath) :

    RequestMessageWithReservations(
        equivalent,
        senderAddresses,
        transactionUUID,
        finalAmountsConfig),
    mNextPathNode(nextNodeInThePath)
{}

CoordinatorReservationRequestMessage::CoordinatorReservationRequestMessage(
    BytesShared buffer) :

    RequestMessageWithReservations(buffer)
{
    size_t currentOffset = RequestMessageWithReservations::kOffsetToInheritedBytes();

    mNextPathNode = deserializeAddress(buffer.get() + currentOffset);
}

BaseAddress::Shared CoordinatorReservationRequestMessage::nextNodeInPath() const {
    return mNextPathNode;
}

const Message::MessageType CoordinatorReservationRequestMessage::typeID() const {
    return Message::Payments_CoordinatorReservationRequest;
}

pair<BytesShared, size_t> CoordinatorReservationRequestMessage::serializeToBytes() const
{
    BytesSerializer serializer;

    serializer.enqueue(RequestMessageWithReservations::serializeToBytes());

    auto serializedAddress = mNextPathNode->serializeToBytes();
    serializer.enqueue(
        serializedAddress,
        mNextPathNode->serializedSize());

    return serializer.collect();
}
