#include "ConfirmChannelMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"
#include "../../../common/serialization/BytesSerializer.h"

ConfirmChannelMessage::ConfirmChannelMessage(
    const TransactionUUID &transactionUUID,
    Contractor::Shared contractor,
    const sphincs::PublicKey::Shared paymentPublicKey):

    TransactionMessage(
        0,
        contractor->ownIdOnContractorSide(),
        transactionUUID),
    mPaymentPublicKey(paymentPublicKey)
{
    encrypt(contractor);
}

ConfirmChannelMessage::ConfirmChannelMessage(
    BytesShared buffer):
    TransactionMessage(buffer)
{
    size_t bytesBufferOffset = TransactionMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, bytesBufferOffset);

    // Deserialize payment public key using SPHINCS+ serialization format.
    const auto kPaymentPublicKeySize = sphincs::PublicKey::keySize();
    auto currentOffset = deserializer.getCurrentOffset();
    mPaymentPublicKey = make_shared<sphincs::PublicKey>(buffer.get() + currentOffset);
    deserializer.skipBytes(kPaymentPublicKeySize);
}

const Message::MessageType ConfirmChannelMessage::typeID() const
{
    return Message::Channel_Confirm;
}

const sphincs::PublicKey::Shared ConfirmChannelMessage::paymentPublicKey() const
{
    return mPaymentPublicKey;
}

pair<BytesShared, size_t> ConfirmChannelMessage::serializeToBytes() const
{
    auto parentBytesAndCount = TransactionMessage::serializeToBytes();

    // Use BytesSerializer for consistent serialization.
    BytesSerializer serializer;
    serializer.enqueue(parentBytesAndCount);
    serializer.copy(mPaymentPublicKey->data(), sphincs::PublicKey::keySize());

    return serializer.collect();
}
