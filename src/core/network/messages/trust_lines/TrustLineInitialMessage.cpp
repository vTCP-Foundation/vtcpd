#include "TrustLineInitialMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"

TrustLineInitialMessage::TrustLineInitialMessage(
    const SerializedEquivalent equivalent,
    Contractor::Shared contractor,
    const TransactionUUID &transactionUUID,
    bool isContractorGateway):

    TransactionMessage(
        equivalent,
        contractor->ownIdOnContractorSide(),
        transactionUUID),
    mIsContractorGateway(isContractorGateway)
{
    encrypt(contractor);
}

TrustLineInitialMessage::TrustLineInitialMessage(
    BytesShared buffer):
    TransactionMessage(buffer)
{
    size_t bytesBufferOffset = TransactionMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, bytesBufferOffset);

    deserializer.copyInto(&mIsContractorGateway);
}


const Message::MessageType TrustLineInitialMessage::typeID() const
{
    return Message::TrustLines_Initial;
}

const bool TrustLineInitialMessage::isContractorGateway() const
{
    return mIsContractorGateway;
}

const bool TrustLineInitialMessage::isCheckCachedResponse() const
{
    return true;
}

pair<BytesShared, size_t> TrustLineInitialMessage::serializeToBytes() const
{
    // todo: use serializer

    auto parentBytesAndCount = TransactionMessage::serializeToBytes();

    size_t bytesCount = parentBytesAndCount.second
                        + sizeof(byte_t);

    BytesShared dataBytesShared = tryCalloc(bytesCount);
    // Use BytesSerializer for consistent serialization
    BytesSerializer serializer;
    serializer.enqueue(parentBytesAndCount);
    serializer.copy(mIsContractorGateway);

    return serializer.collect();
}
