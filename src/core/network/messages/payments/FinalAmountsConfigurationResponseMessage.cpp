#include "FinalAmountsConfigurationResponseMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"
#include "../../../common/serialization/BytesSerializer.h"

FinalAmountsConfigurationResponseMessage::FinalAmountsConfigurationResponseMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID& transactionUUID,
    const OperationState state) :

    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID),
    mState(state)
{}

FinalAmountsConfigurationResponseMessage::FinalAmountsConfigurationResponseMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID& transactionUUID,
    const OperationState state,
    const lamport::PublicKey::Shared publicKey) :

    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID),
    mState(state),
    mPublicKey(publicKey)
{}

FinalAmountsConfigurationResponseMessage::FinalAmountsConfigurationResponseMessage(
    BytesShared buffer):

    TransactionMessage(buffer)
{
    size_t currentOffset = TransactionMessage::kOffsetToInheritedBytes();
    //----------------------------------------------------
    BytesDeserializer deserializer(buffer, currentOffset);
    SerializedOperationState state;
    deserializer.copyInto(&state);
    mState = (OperationState) state;
    if (mState == Accepted) {
        auto publicKey = make_shared<lamport::PublicKey>(
                             buffer.get() + currentOffset + sizeof(SerializedOperationState));
        mPublicKey = publicKey;
    }
}

const FinalAmountsConfigurationResponseMessage::OperationState FinalAmountsConfigurationResponseMessage::state() const
{
    return mState;
}

const lamport::PublicKey::Shared FinalAmountsConfigurationResponseMessage::publicKey() const
{
    return mPublicKey;
}

pair<BytesShared, size_t> FinalAmountsConfigurationResponseMessage::serializeToBytes() const
{
    BytesSerializer serializer;
    serializer.enqueue(TransactionMessage::serializeToBytes());
    serializer.copy((SerializedOperationState)mState);
    //----------------------------------------------------
    if (mState == Accepted) {
        serializer.copy(
            mPublicKey->data(),
            mPublicKey->keySize());
    }
    return serializer.collect();
}

const Message::MessageType FinalAmountsConfigurationResponseMessage::typeID() const
{
    return Message::Payments_FinalAmountsConfigurationResponse;
}