#include "TransactionPublicKeyHashMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"

// Constructor without receipts
TransactionPublicKeyHashMessage::TransactionPublicKeyHashMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    const PaymentNodeID paymentNodeID,
    const sphincs::KeyHash::Shared transactionPublicKeyHash) :

    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID),
    mPaymentNodeID(paymentNodeID),
    mTransactionPublicKeyHash(transactionPublicKeyHash)
    // mSignatures empty
{
}

// NEW: Constructor with multiple receipts
TransactionPublicKeyHashMessage::TransactionPublicKeyHashMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    const PaymentNodeID paymentNodeID,
    const sphincs::KeyHash::Shared transactionPublicKeyHash,
    const vector<pair<SerializedEquivalent, sphincs::Signature::Shared>> &signatures) :

    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID),
    mPaymentNodeID(paymentNodeID),
    mTransactionPublicKeyHash(transactionPublicKeyHash),
    mSignatures(signatures)
{
}

// DEPRECATED: Constructor with single receipt (backward compatibility)
TransactionPublicKeyHashMessage::TransactionPublicKeyHashMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    const PaymentNodeID paymentNodeID,
    const sphincs::KeyHash::Shared transactionPublicKeyHash,
    const sphincs::Signature::Shared signature) :

    TransactionMessage(
        equivalent,
        senderAddresses,
        transactionUUID),
    mPaymentNodeID(paymentNodeID),
    mTransactionPublicKeyHash(transactionPublicKeyHash)
{
    // Create single-element vector with mEquivalent and signature
    if (signature) {
        mSignatures.emplace_back(equivalent, signature);
    }
    // If signature is null, mSignatures remains empty
}

TransactionPublicKeyHashMessage::TransactionPublicKeyHashMessage(
    BytesShared buffer) : TransactionMessage(buffer)
{
    auto bytesBufferOffset = TransactionMessage::kOffsetToInheritedBytes();
    BytesDeserializer deserializer(buffer, bytesBufferOffset);

    deserializer.copyInto(&mPaymentNodeID);

    // Get current offset for crypto key creation
    auto currentOffset = deserializer.getCurrentOffset();
    mTransactionPublicKeyHash = make_shared<sphincs::KeyHash>(buffer.get() + currentOffset);
    deserializer.skipBytes(sphincs::KeyHash::kBytesSize);

    // Deserialize signatures count
    SerializedRecordsCount signaturesCount;
    deserializer.copyInto(&signaturesCount);

    mSignatures.reserve(signaturesCount);

    // Deserialize each (equivalent, signature) pair
    for (SerializedRecordNumber idx = 0; idx < signaturesCount; idx++) {
        // Deserialize equivalent
        SerializedEquivalent equivalent;
        deserializer.copyInto(&equivalent);

        // Deserialize signature
        currentOffset = deserializer.getCurrentOffset();
        auto signature = make_shared<sphincs::Signature>(buffer.get() + currentOffset);
        deserializer.skipBytes(signature->signatureSize());

        mSignatures.emplace_back(equivalent, signature);
    }
}

const Message::MessageType TransactionPublicKeyHashMessage::typeID() const
{
    return Message::Payments_TransactionPublicKeyHash;
}

const PaymentNodeID TransactionPublicKeyHashMessage::paymentNodeID() const
{
    return mPaymentNodeID;
}

const sphincs::KeyHash::Shared TransactionPublicKeyHashMessage::transactionPublicKeyHash() const
{
    return mTransactionPublicKeyHash;
}

bool TransactionPublicKeyHashMessage::isReceiptContains() const
{
    return !mSignatures.empty();
}

const vector<pair<SerializedEquivalent, sphincs::Signature::Shared>>&
TransactionPublicKeyHashMessage::signatures() const
{
    return mSignatures;
}

pair<BytesShared, size_t> TransactionPublicKeyHashMessage::serializeToBytes() const
{
    // TODO: Performance optimization - parent data is serialized twice:
    // 1. TransactionMessage::serializeToBytes() creates buffer
    // 2. serializer.enqueue() copies that buffer again
    // Consider passing serializer down inheritance chain to avoid redundant allocation/copy
    const auto parentBytesAndCount = TransactionMessage::serializeToBytes();

    // Use BytesSerializer for consistent serialization
    BytesSerializer serializer;
    serializer.enqueue(parentBytesAndCount);
    serializer.copy(mPaymentNodeID);
    serializer.copy(mTransactionPublicKeyHash->data(), sphincs::KeyHash::kBytesSize);

    // Serialize signatures count
    serializer.copy(static_cast<SerializedRecordsCount>(mSignatures.size()));

    // Serialize each (equivalent, signature) pair
    for (const auto& [equivalent, signature] : mSignatures) {
        // Serialize equivalent
        serializer.copy(equivalent);

        // Serialize signature
        serializer.copy(signature->data(), signature->signatureSize());
    }

    return serializer.collect();
}