#include "ConfirmationMessage.h"
#include "../../../../common/serialization/BytesDeserializer.h"
#include "../../../../common/serialization/BytesSerializer.h"

ConfirmationMessage::ConfirmationMessage(
    const SerializedEquivalent equivalent,
    const TransactionUUID &transactionUUID,
    const OperationState state) :

    TransactionMessage(
        equivalent,
        transactionUUID),
    mState(state)
{}

ConfirmationMessage::ConfirmationMessage(
    const SerializedEquivalent equivalent,
    ContractorID idOnReceiverSide,
    const TransactionUUID &transactionUUID,
    const OperationState state) :

    TransactionMessage(
        equivalent,
        idOnReceiverSide,
        transactionUUID),
    mState(state)
{}

ConfirmationMessage::ConfirmationMessage(
    BytesShared buffer):

    TransactionMessage(buffer)
{
    size_t currentOffset = TransactionMessage::kOffsetToInheritedBytes();
    //----------------------------------------------------
    BytesDeserializer deserializer(buffer, currentOffset);
    SerializedOperationState state;
    deserializer.copyInto(&state);
    mState = (OperationState) state;
}

const Message::MessageType ConfirmationMessage::typeID() const
{
    return Message::System_Confirmation;
}

const ConfirmationMessage::OperationState ConfirmationMessage::state() const
{
    return mState;
}

pair<BytesShared, size_t> ConfirmationMessage::serializeToBytes() const
{
    // TODO: Serialization architecture optimization needed across entire inheritance chain:
    // This base class pattern causes redundant parent serialization in all child classes.
    // Each child calls parent::serializeToBytes() then copies that buffer again via enqueue().
    // Consider: void serializeToSerializer(BytesSerializer& serializer) pattern to avoid copies.
    BytesSerializer serializer;
    serializer.enqueue(TransactionMessage::serializeToBytes());
    serializer.copy((SerializedOperationState)mState);
    return serializer.collect();
}

const size_t ConfirmationMessage::kOffsetToInheritedBytes() const
{
    const auto kOffset =
        TransactionMessage::kOffsetToInheritedBytes()
        + BytesSerializer::kSerializedByteSize;

    return kOffset;
}
