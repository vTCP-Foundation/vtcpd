#include "InitChannelMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"

InitChannelMessage::InitChannelMessage(
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID &transactionUUID,
    Contractor::Shared contractor):

    TransactionMessage(
        0,
        senderAddresses,
        transactionUUID),
    mContractorID(contractor->getID()),
    mPublicKey(contractor->cryptoKey()->publicKey)
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

pair<BytesShared, size_t> InitChannelMessage::serializeToBytes() const
{
    // todo: use serializer

    auto parentBytesAndCount = TransactionMessage::serializeToBytes();

    size_t bytesCount = parentBytesAndCount.second
                        + sizeof(ContractorID)
                        + mPublicKey->kBytesSize;

    // Use BytesSerializer for consistent serialization
    BytesSerializer serializer;
    serializer.enqueue(parentBytesAndCount);
    serializer.copy(mContractorID);
    serializer.copy(&mPublicKey->key, mPublicKey->kBytesSize);

    return serializer.collect();
}