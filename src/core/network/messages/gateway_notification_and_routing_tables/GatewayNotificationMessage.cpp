#include "GatewayNotificationMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"
#include "../../../common/serialization/BytesSerializer.h"

GatewayNotificationMessage::GatewayNotificationMessage(
    ContractorID idOnReceiverSide,
    const TransactionUUID& transactionUUID,
    const vector<SerializedEquivalent> gatewayEquivalents) :

    TransactionMessage(
        0,
        idOnReceiverSide,
        transactionUUID),
    mGatewayEquivalents(gatewayEquivalents)
{}

GatewayNotificationMessage::GatewayNotificationMessage(
    BytesShared buffer):

    TransactionMessage(buffer)
{
    size_t currentOffset = TransactionMessage::kOffsetToInheritedBytes();
    //----------------------------------------------------
    BytesDeserializer deserializer(buffer, currentOffset);
    SerializedRecordsCount equivalentGatewaysCount;
    deserializer.copyInto(&equivalentGatewaysCount);
    //-----------------------------------------------------
    mGatewayEquivalents.reserve(equivalentGatewaysCount);
    for (SerializedRecordNumber idx = 0; idx < equivalentGatewaysCount; idx++) {
        SerializedEquivalent gatewayEquivalent;
        deserializer.copyInto(&gatewayEquivalent);
        //---------------------------------------------------
        mGatewayEquivalents.push_back(
            gatewayEquivalent);
    }
}

const vector<SerializedEquivalent> GatewayNotificationMessage::gatewayEquivalents() const
{
    return mGatewayEquivalents;
}

pair<BytesShared, size_t> GatewayNotificationMessage::serializeToBytes() const
{
    BytesSerializer serializer;
    serializer.enqueue(TransactionMessage::serializeToBytes());
    serializer.copy((SerializedRecordsCount)mGatewayEquivalents.size());
    //----------------------------------------------------
    for (auto const &gatewayEquivalent : mGatewayEquivalents) {
        serializer.copy(gatewayEquivalent);
    }
    //----------------------------------------------------
    return serializer.collect();
}

const Message::MessageType GatewayNotificationMessage::typeID() const
{
    return Message::GatewayNotification;
}

const bool GatewayNotificationMessage::isAddToConfirmationRequiredMessagesHandler() const
{
    return true;
}