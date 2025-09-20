#include "MaxFlowCalculationConfirmationMessage.h"
#include "../../../../common/serialization/BytesDeserializer.h"
#include "../../../../common/serialization/BytesSerializer.h"

MaxFlowCalculationConfirmationMessage::MaxFlowCalculationConfirmationMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    const ConfirmationID confirmationID) :

    SenderMessage(
        equivalent,
        senderAddresses),
    mConfirmationID(confirmationID)
{}

MaxFlowCalculationConfirmationMessage::MaxFlowCalculationConfirmationMessage(
    BytesShared buffer):

    SenderMessage(buffer)
{
    auto currentOffset = SenderMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, currentOffset);
    deserializer.copyInto(&mConfirmationID);
}

void MaxFlowCalculationConfirmationMessage::setConfirmationID(
    const ConfirmationID confirmationID)
{
    mConfirmationID = confirmationID;
}

const Message::MessageType MaxFlowCalculationConfirmationMessage::typeID() const
{
    return Message::MaxFlow_Confirmation;
}

const ConfirmationID MaxFlowCalculationConfirmationMessage::confirmationID() const
{
    return mConfirmationID;
}

pair<BytesShared, size_t> MaxFlowCalculationConfirmationMessage::serializeToBytes() const
{
    // TODO: Serialization architecture optimization needed across entire inheritance chain:
    // This base class pattern causes redundant parent serialization in all child classes.
    // Each child calls parent::serializeToBytes() then copies that buffer again via enqueue().
    // Consider: void serializeToSerializer(BytesSerializer& serializer) pattern to avoid copies.
    // NOTE: This method also has redundant parentBytesAndCount variable (serializes parent twice!)
    auto parentBytesAndCount = SenderMessage::serializeToBytes();

    BytesSerializer serializer;
    serializer.enqueue(SenderMessage::serializeToBytes());
    serializer.copy(mConfirmationID);
    return serializer.collect();
}

const size_t MaxFlowCalculationConfirmationMessage::kOffsetToInheritedBytes() const
{
    const auto kOffset =
        SenderMessage::kOffsetToInheritedBytes()
        + BytesSerializer::kSerializedUInt16Size;
    return kOffset;
}
