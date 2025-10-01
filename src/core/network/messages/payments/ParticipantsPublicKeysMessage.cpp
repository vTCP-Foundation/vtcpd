#include "ParticipantsPublicKeysMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"

ParticipantsPublicKeysMessage::ParticipantsPublicKeysMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    const map<PaymentNodeID, sphincs::PublicKey::Shared> &publicKeys):
    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID),
    mPublicKeys(publicKeys)
{}

ParticipantsPublicKeysMessage::ParticipantsPublicKeysMessage(
    BytesShared buffer) :
    TransactionMessage(buffer)
{
    auto bytesBufferOffset = TransactionMessage::kOffsetToInheritedBytes();

    BytesDeserializer deserializer(buffer, bytesBufferOffset);
    SerializedRecordsCount kRecordsCount;
    deserializer.copyInto(&kRecordsCount);

    for (SerializedRecordNumber i = 0; i < kRecordsCount; ++i) {
        PaymentNodeID paymentNodeID;
        deserializer.copyInto(&paymentNodeID);

        // Get current offset for crypto key creation
        auto currentOffset = deserializer.getCurrentOffset();
        auto publicKey = make_shared<sphincs::PublicKey>(buffer.get() + currentOffset);
        deserializer.skipBytes(sphincs::PublicKey::keySize());

        mPublicKeys.insert(
            make_pair(
                paymentNodeID,
                publicKey));
    }
}

const Message::MessageType ParticipantsPublicKeysMessage::typeID() const
{
    return Message::Payments_ParticipantsPublicKeys;
}

const map<PaymentNodeID, sphincs::PublicKey::Shared>& ParticipantsPublicKeysMessage::publicKeys() const
{
    return mPublicKeys;
}

pair<BytesShared, size_t> ParticipantsPublicKeysMessage::serializeToBytes() const
{
    // TODO: Performance optimization - parent data is serialized twice:
    // 1. TransactionMessage::serializeToBytes() creates buffer
    // 2. serializer.enqueue() copies that buffer again
    // Consider passing serializer down inheritance chain to avoid redundant allocation/copy
    const auto parentBytesAndCount = TransactionMessage::serializeToBytes();

    const auto kBufferSize =
        parentBytesAndCount.second
        + sizeof(SerializedRecordsCount)
        + mPublicKeys.size()
        * (sizeof(PaymentNodeID) + sphincs::PublicKey::keySize());

    // Use BytesSerializer for consistent serialization
    BytesSerializer serializer;
    serializer.enqueue(parentBytesAndCount);

    // Records count
    auto kTotalParticipantsCount = mPublicKeys.size();
    serializer.copy((SerializedRecordsCount)kTotalParticipantsCount);

    // Nodes IDs and publicKeys
    for (const auto &nodeIDAndPublicKey : mPublicKeys) {
        serializer.copy(nodeIDAndPublicKey.first);
        serializer.copy(nodeIDAndPublicKey.second->data(), sphincs::PublicKey::keySize());
    }

    return serializer.collect();
}