#include "FinalPathCycleConfigurationMessage.h"
#include "../../../common/serialization/BytesDeserializer.h"
#include "../../../common/serialization/BytesSerializer.h"

FinalPathCycleConfigurationMessage::FinalPathCycleConfigurationMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    const TrustLineAmount &amount,
    const map<PaymentNodeID, Contractor::Shared> &paymentParticipants,
    const BlockNumber maximalClaimingBlockNumber) :

    RequestCycleMessage(
        equivalent,
        senderAddresses,
        transactionUUID,
        amount),
    mPaymentParticipants(paymentParticipants),
    mMaximalClaimingBlockNumber(maximalClaimingBlockNumber),
    mIsReceiptContains(false)
{
}

FinalPathCycleConfigurationMessage::FinalPathCycleConfigurationMessage(
    const SerializedEquivalent equivalent,
    vector<BaseAddress::Shared> &senderAddresses,
    const TransactionUUID &transactionUUID,
    const TrustLineAmount &amount,
    const map<PaymentNodeID, Contractor::Shared> &paymentParticipants,
    const BlockNumber maximalClaimingBlockNumber,
    const sphincs::Signature::Shared signature,
    const sphincs::KeyHash::Shared transactionPublicKeyHash) :

    RequestCycleMessage(
        equivalent,
        senderAddresses,
        transactionUUID,
        amount),
    mPaymentParticipants(paymentParticipants),
    mMaximalClaimingBlockNumber(maximalClaimingBlockNumber),
    mIsReceiptContains(true),
    mSignature(signature),
    mTransactionPublicKeyHash(transactionPublicKeyHash)
{
}

FinalPathCycleConfigurationMessage::FinalPathCycleConfigurationMessage(
    BytesShared buffer) :

    RequestCycleMessage(buffer)
{
    BytesDeserializer deserializer(
        buffer,
        RequestCycleMessage::kOffsetToInheritedBytes());

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
        deserializer.skipBytes(sphincs::Signature::signatureSize());
        mSignature = signature;

        mTransactionPublicKeyHash = make_shared<sphincs::KeyHash>(
            buffer.get() + deserializer.getCurrentOffset());
        deserializer.skipBytes(sphincs::KeyHash::kBytesSize);
    }
}

const Message::MessageType FinalPathCycleConfigurationMessage::typeID() const
{
    return Message::Payments_FinalPathCycleConfiguration;
}

const map<PaymentNodeID, Contractor::Shared> &FinalPathCycleConfigurationMessage::paymentParticipants() const
{
    return mPaymentParticipants;
}

const BlockNumber FinalPathCycleConfigurationMessage::maximalClaimingBlockNumber() const
{
    return mMaximalClaimingBlockNumber;
}

bool FinalPathCycleConfigurationMessage::isReceiptContains() const
{
    return mIsReceiptContains;
}

const sphincs::Signature::Shared FinalPathCycleConfigurationMessage::signature() const
{
    return mSignature;
}

const sphincs::KeyHash::Shared FinalPathCycleConfigurationMessage::transactionPublicKeyHash() const
{
    return mTransactionPublicKeyHash;
}

pair<BytesShared, size_t> FinalPathCycleConfigurationMessage::serializeToBytes() const
{
    BytesSerializer serializer;

    serializer.enqueue(RequestCycleMessage::serializeToBytes());
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
        serializer.copy(
            mTransactionPublicKeyHash->data(),
            sphincs::KeyHash::kBytesSize);
    }

    return serializer.collect();
}
