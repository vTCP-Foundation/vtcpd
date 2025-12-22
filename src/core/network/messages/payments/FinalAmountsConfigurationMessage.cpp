#include "FinalAmountsConfigurationMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"
#include "../../../common/serialization/BytesSerializer.h"

// Constructor without receipts
FinalAmountsConfigurationMessage::FinalAmountsConfigurationMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID &transactionUUID,
    const vector<pair<PathID, ConstSharedTrustLineAmount>> &finalAmountsConfig,
    const map<PaymentNodeID, Contractor::Shared> &paymentParticipants,
    const BlockNumber maximalClaimingBlockNumber) :

    RequestMessageWithReservations(
        equivalent,
        senderAddresses,
        transactionUUID,
        finalAmountsConfig),
    mPaymentParticipants(paymentParticipants),
    mMaximalClaimingBlockNumber(maximalClaimingBlockNumber)
    // mSignatures empty
{
}

// NEW: Constructor with PathReservation vector (no receipts)
FinalAmountsConfigurationMessage::FinalAmountsConfigurationMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID &transactionUUID,
    const vector<PathReservation> &finalAmountsConfig,
    const map<PaymentNodeID, Contractor::Shared> &paymentParticipants,
    const BlockNumber maximalClaimingBlockNumber,
    const uint16_t disputeGracePeriodBlocksCount) :

    RequestMessageWithReservations(
        equivalent,
        senderAddresses,
        transactionUUID,
        finalAmountsConfig),
    mPaymentParticipants(paymentParticipants),
    mMaximalClaimingBlockNumber(maximalClaimingBlockNumber),
    mDisputeGracePeriodBlocksCount(disputeGracePeriodBlocksCount)
    // mSignatures empty
{
}

// NEW: Constructor with PathReservation vector and multiple receipts
FinalAmountsConfigurationMessage::FinalAmountsConfigurationMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID &transactionUUID,
    const vector<PathReservation> &finalAmountsConfig,
    const map<PaymentNodeID, Contractor::Shared> &paymentParticipants,
    const BlockNumber maximalClaimingBlockNumber,
    const uint16_t disputeGracePeriodBlocksCount,
    const vector<pair<SerializedEquivalent, sphincs::Signature::Shared>> &signatures) :

    RequestMessageWithReservations(
        equivalent,
        senderAddresses,
        transactionUUID,
        finalAmountsConfig),
    mPaymentParticipants(paymentParticipants),
    mMaximalClaimingBlockNumber(maximalClaimingBlockNumber),
    mDisputeGracePeriodBlocksCount(disputeGracePeriodBlocksCount),
    mSignatures(signatures)
{
}

// NEW: Constructor with PathReservation vector and single receipt
FinalAmountsConfigurationMessage::FinalAmountsConfigurationMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID &transactionUUID,
    const vector<PathReservation> &finalAmountsConfig,
    const map<PaymentNodeID, Contractor::Shared> &paymentParticipants,
    const BlockNumber maximalClaimingBlockNumber,
    const uint16_t disputeGracePeriodBlocksCount,
    const sphincs::Signature::Shared signature) :

    RequestMessageWithReservations(
        equivalent,
        senderAddresses,
        transactionUUID,
        finalAmountsConfig),
    mPaymentParticipants(paymentParticipants),
    mMaximalClaimingBlockNumber(maximalClaimingBlockNumber),
    mDisputeGracePeriodBlocksCount(disputeGracePeriodBlocksCount)
{
    // Create single-element vector with mEquivalent and signature
    if (signature) {
        mSignatures.emplace_back(equivalent, signature);
    }
    // If signature is null, mSignatures remains empty
}

// NEW: Constructor with multiple receipts
FinalAmountsConfigurationMessage::FinalAmountsConfigurationMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID &transactionUUID,
    const vector<pair<PathID, ConstSharedTrustLineAmount>> &finalAmountsConfig,
    const map<PaymentNodeID, Contractor::Shared> &paymentParticipants,
    const BlockNumber maximalClaimingBlockNumber,
    const uint16_t disputeGracePeriodBlocksCount,
    const vector<pair<SerializedEquivalent, sphincs::Signature::Shared>> &signatures) :

    RequestMessageWithReservations(
        equivalent,
        senderAddresses,
        transactionUUID,
        finalAmountsConfig),
    mPaymentParticipants(paymentParticipants),
    mMaximalClaimingBlockNumber(maximalClaimingBlockNumber),
    mDisputeGracePeriodBlocksCount(disputeGracePeriodBlocksCount),
    mSignatures(signatures)
{
}

// DEPRECATED: Constructor with single receipt (backward compatibility)
FinalAmountsConfigurationMessage::FinalAmountsConfigurationMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> senderAddresses,
    const TransactionUUID &transactionUUID,
    const vector<pair<PathID, ConstSharedTrustLineAmount>> &finalAmountsConfig,
    const map<PaymentNodeID, Contractor::Shared> &paymentParticipants,
    const BlockNumber maximalClaimingBlockNumber,
    const sphincs::Signature::Shared signature) :

    RequestMessageWithReservations(
        equivalent,
        senderAddresses,
        transactionUUID,
        finalAmountsConfig),
    mPaymentParticipants(paymentParticipants),
    mMaximalClaimingBlockNumber(maximalClaimingBlockNumber)
{
    // Create single-element vector with mEquivalent and signature
    if (signature) {
        mSignatures.emplace_back(equivalent, signature);
    }
    // If signature is null, mSignatures remains empty
}

FinalAmountsConfigurationMessage::FinalAmountsConfigurationMessage(
    BytesShared buffer) : RequestMessageWithReservations(buffer)
{
    BytesDeserializer deserializer(
        buffer,
        RequestMessageWithReservations::kOffsetToInheritedBytes());

    SerializedRecordsCount paymentParticipantsCount;
    deserializer.copyInto(&paymentParticipantsCount);

    for (SerializedRecordNumber idx = 0; idx < paymentParticipantsCount; idx++) {
        PaymentNodeID paymentNodeID;
        deserializer.copyInto(&paymentNodeID);

        auto contractor = make_shared<Contractor>(
            buffer.get() + deserializer.getCurrentOffset());
        deserializer.skipBytes(contractor->serializedSize());

        mPaymentParticipants.emplace(paymentNodeID, contractor);
    }

    deserializer.copyInto(&mMaximalClaimingBlockNumber);

    deserializer.copyInto(&mDisputeGracePeriodBlocksCount);

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
        auto signature = make_shared<sphincs::Signature>(
            buffer.get() + deserializer.getCurrentOffset());
        deserializer.skipBytes(signature->signatureSize());

        mSignatures.emplace_back(equivalent, signature);
    }
}

const Message::MessageType FinalAmountsConfigurationMessage::typeID() const
{
    return Message::Payments_FinalAmountsConfiguration;
}

const map<PaymentNodeID, Contractor::Shared> &FinalAmountsConfigurationMessage::paymentParticipants() const
{
    return mPaymentParticipants;
}

const BlockNumber FinalAmountsConfigurationMessage::maximalClaimingBlockNumber() const
{
    return mMaximalClaimingBlockNumber;
}

const uint16_t FinalAmountsConfigurationMessage::disputeGracePeriodBlocksCount() const
{
    return mDisputeGracePeriodBlocksCount;
}

bool FinalAmountsConfigurationMessage::isReceiptContains() const
{
    return !mSignatures.empty();
}

const vector<pair<SerializedEquivalent, sphincs::Signature::Shared>>&
FinalAmountsConfigurationMessage::signatures() const
{
    return mSignatures;
}

pair<BytesShared, size_t> FinalAmountsConfigurationMessage::serializeToBytes() const
{
    BytesSerializer serializer;

    serializer.enqueue(RequestMessageWithReservations::serializeToBytes());
    serializer.copy(static_cast<SerializedRecordsCount>(mPaymentParticipants.size()));

    for (const auto &paymentNodeIdAndContractor : mPaymentParticipants) {
        serializer.copy(paymentNodeIdAndContractor.first);
        serializer.enqueue(
            paymentNodeIdAndContractor.second->serializeToBytes(),
            paymentNodeIdAndContractor.second->serializedSize());
    }

    serializer.copy(mMaximalClaimingBlockNumber);

    serializer.copy(mDisputeGracePeriodBlocksCount);

    // Serialize signatures count
    serializer.copy(static_cast<SerializedRecordsCount>(mSignatures.size()));

    // Serialize each (equivalent, signature) pair
    for (const auto& [equivalent, signature] : mSignatures) {
        // Serialize equivalent
        serializer.copy(equivalent);

        // Serialize signature
        serializer.copy(
            signature->data(),
            signature->signatureSize());
    }

    return serializer.collect();
}
