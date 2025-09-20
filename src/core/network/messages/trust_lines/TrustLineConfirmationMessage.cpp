#include "TrustLineConfirmationMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"

TrustLineConfirmationMessage::TrustLineConfirmationMessage(
    const SerializedEquivalent equivalent,
    Contractor::Shared contractor,
    const TransactionUUID &transactionUUID,
    bool isContractorGateway,
    const OperationState state) :

    ConfirmationMessage(
        equivalent,
        contractor->ownIdOnContractorSide(),
        transactionUUID,
        state),
    mIsContractorGateway(isContractorGateway)
{
    encrypt(contractor);
}

TrustLineConfirmationMessage::TrustLineConfirmationMessage(
    BytesShared buffer):

    ConfirmationMessage(buffer)
{
    size_t bytesBufferOffset = ConfirmationMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, bytesBufferOffset);

    deserializer.copyInto(&mIsContractorGateway);
}

const Message::MessageType TrustLineConfirmationMessage::typeID() const
{
    return Message::TrustLines_Confirmation;
}

const bool TrustLineConfirmationMessage::isContractorGateway() const
{
    return mIsContractorGateway;
}

pair<BytesShared, size_t> TrustLineConfirmationMessage::serializeToBytes() const
{
    auto parentBytesAndCount = ConfirmationMessage::serializeToBytes();

    size_t bytesCount =
        parentBytesAndCount.second
        + sizeof(byte_t);

    // Use BytesSerializer for consistent serialization
    BytesSerializer serializer;
    serializer.enqueue(parentBytesAndCount);
    serializer.copy(mIsContractorGateway);

    return serializer.collect();
}