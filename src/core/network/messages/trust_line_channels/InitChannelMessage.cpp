#include "InitChannelMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"

InitChannelMessage::InitChannelMessage(
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID &transactionUUID,
    Contractor::Shared contractor,
    const sphincs::PublicKey::Shared paymentPublicKey):

    TransactionMessage(
        0,
        senderAddresses,
        transactionUUID),
    mContractorID(contractor->getID()),
    mPublicKey(contractor->cryptoKey()->publicKey),
    mPaymentPublicKey(paymentPublicKey)
{
    encrypt(contractor);
}

InitChannelMessage::InitChannelMessage(
    BytesShared buffer):
    TransactionMessage(buffer)
{
    size_t bytesBufferOffset = TransactionMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, bytesBufferOffset);

    deserializer.copyInto(&mContractorID);

    mPublicKey = make_shared<MsgEncryptor::PublicKey>();
    deserializer.copyInto(mPublicKey->key, mPublicKey->kBytesSize);

    // Deserialize payment public key using SPHINCS+ serialization format.
    const auto kPaymentPublicKeySize = sphincs::PublicKey::keySize();
    auto currentOffset = deserializer.getCurrentOffset();
    mPaymentPublicKey = make_shared<sphincs::PublicKey>(buffer.get() + currentOffset);
    deserializer.skipBytes(kPaymentPublicKeySize);
}


const Message::MessageType InitChannelMessage::typeID() const
{
    return Message::Channel_Init;
}

const ContractorID InitChannelMessage::contractorID() const
{
    return mContractorID;
}

const MsgEncryptor::PublicKey::Shared InitChannelMessage::publicKey() const
{
    return mPublicKey;
}

const sphincs::PublicKey::Shared InitChannelMessage::paymentPublicKey() const
{
    return mPaymentPublicKey;
}

pair<BytesShared, size_t> InitChannelMessage::serializeToBytes() const
{
    auto parentBytesAndCount = TransactionMessage::serializeToBytes();

    // Use BytesSerializer for consistent serialization
    BytesSerializer serializer;
    serializer.enqueue(parentBytesAndCount);
    serializer.copy(mContractorID);
    serializer.copy(&mPublicKey->key, mPublicKey->kBytesSize);
    // Serialize payment public key after channel crypto key.
    serializer.copy(mPaymentPublicKey->data(), sphincs::PublicKey::keySize());

    return serializer.collect();
}
