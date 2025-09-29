#include "ReceiverInitPaymentRequestMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"
#include "../../../common/serialization/BytesSerializer.h"

ReceiverInitPaymentRequestMessage::ReceiverInitPaymentRequestMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    const TrustLineAmount &amount,
    const string payload) :
    RequestMessage(
        equivalent,
        senderAddresses,
        transactionUUID,
        0,
        amount),
    mPayload(payload)
{}

ReceiverInitPaymentRequestMessage::ReceiverInitPaymentRequestMessage(
    BytesShared buffer) :
    RequestMessage(
        buffer)
{
    auto currentOffset = RequestMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, currentOffset);
    PayloadLength payloadLength;
    deserializer.copyInto(&payloadLength);
    if (payloadLength > 0) {
        mPayload = string(
                       buffer.get() + currentOffset + sizeof(PayloadLength),
                       buffer.get() + currentOffset + sizeof(PayloadLength) + payloadLength);
    } else {
        mPayload = "";
    }
}

const Message::MessageType ReceiverInitPaymentRequestMessage::typeID() const
{
    return Message::Payments_ReceiverInitPaymentRequest;
}

const string ReceiverInitPaymentRequestMessage::payload() const
{
    return mPayload;
}

pair<BytesShared, size_t> ReceiverInitPaymentRequestMessage::serializeToBytes() const
{
    BytesSerializer serializer;
    serializer.enqueue(RequestMessage::serializeToBytes());
    serializer.copy((PayloadLength)mPayload.length());

    if (mPayload.length() > 0) {
        serializer.copy(
            mPayload.c_str(),
            mPayload.length());
    }

    return serializer.collect();
}
