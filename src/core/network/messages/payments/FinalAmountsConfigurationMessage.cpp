#include "FinalAmountsConfigurationMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"
#include "../../../common/serialization/BytesSerializer.h"

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
    mMaximalClaimingBlockNumber(maximalClaimingBlockNumber),
    mIsReceiptContains(false)
{
}

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
    mMaximalClaimingBlockNumber(maximalClaimingBlockNumber),
    mIsReceiptContains(true),
    mSignature(signature)
{
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
    deserializer.copyInto(&mIsReceiptContains);

    if (mIsReceiptContains) {
        auto signature = make_shared<sphincs::Signature>(
            buffer.get() + deserializer.getCurrentOffset());
        mSignature = signature;
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

bool FinalAmountsConfigurationMessage::isReceiptContains() const
{
    return mIsReceiptContains;
}

const sphincs::Signature::Shared FinalAmountsConfigurationMessage::signature() const
{
    return mSignature;
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
    serializer.copy(static_cast<byte_t>(mIsReceiptContains));

    if (mIsReceiptContains) {
        serializer.copy(
            mSignature->data(),
            mSignature->signatureSize());
    }

    return serializer.collect();
}
