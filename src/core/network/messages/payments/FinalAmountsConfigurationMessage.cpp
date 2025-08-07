#include "FinalAmountsConfigurationMessage.h"

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
    auto parentMessageOffset = RequestMessageWithReservations::kOffsetToInheritedBytes();
    auto bytesBufferOffset = buffer.get() + parentMessageOffset;
    //----------------------------------------------------
    auto *paymentParticipantsCount = new (bytesBufferOffset) SerializedRecordsCount;
    bytesBufferOffset += sizeof(SerializedRecordsCount);
    //-----------------------------------------------------
    for (SerializedRecordNumber idx = 0; idx < *paymentParticipantsCount; idx++) {
        auto *paymentNodeID = new (bytesBufferOffset) PaymentNodeID;
        bytesBufferOffset += sizeof(PaymentNodeID);
        //---------------------------------------------------
        auto contractor = make_shared<Contractor>(bytesBufferOffset);
        bytesBufferOffset += contractor->serializedSize();
        //---------------------------------------------------
        mPaymentParticipants.insert(
            make_pair(
                *paymentNodeID,
                contractor));
    }
    //----------------------------------------------------
    memcpy(
        &mMaximalClaimingBlockNumber,
        bytesBufferOffset,
        sizeof(BlockNumber));
    bytesBufferOffset += sizeof(BlockNumber);
    //----------------------------------------------------
    memcpy(
        &mIsReceiptContains,
        bytesBufferOffset,
        sizeof(byte_t));
    //----------------------------------------------------
    if (mIsReceiptContains) {
        bytesBufferOffset += sizeof(byte_t);
        auto signature = make_shared<sphincs::Signature>(
                             bytesBufferOffset);
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
    auto parentBytesAndCount = RequestMessageWithReservations::serializeToBytes();
    size_t bytesCount = parentBytesAndCount.second + sizeof(SerializedRecordsCount) + sizeof(BlockNumber) + sizeof(byte_t);
    for (const auto &participant : mPaymentParticipants) {
        bytesCount += sizeof(PaymentNodeID) + participant.second->serializedSize();
    }
    if (mIsReceiptContains) {
        bytesCount += sphincs::Signature::signatureSize();
    }

    BytesShared buffer = tryMalloc(bytesCount);

    auto initialOffset = buffer.get();
    memcpy(
        initialOffset,
        parentBytesAndCount.first.get(),
        parentBytesAndCount.second);
    auto bytesBufferOffset = initialOffset + parentBytesAndCount.second;

    //----------------------------------------------------
    auto paymentParticipantsCount = (SerializedRecordsCount)mPaymentParticipants.size();
    memcpy(
        bytesBufferOffset,
        &paymentParticipantsCount,
        sizeof(SerializedRecordsCount));
    bytesBufferOffset += sizeof(SerializedRecordsCount);
    //----------------------------------------------------
    for (auto const &paymentNodeIdAndContractor : mPaymentParticipants) {
        memcpy(
            bytesBufferOffset,
            &paymentNodeIdAndContractor.first,
            sizeof(PaymentNodeID));
        bytesBufferOffset += sizeof(PaymentNodeID);

        auto contractorSerializedData = paymentNodeIdAndContractor.second->serializeToBytes();
        memcpy(
            bytesBufferOffset,
            contractorSerializedData.get(),
            paymentNodeIdAndContractor.second->serializedSize());
        bytesBufferOffset += paymentNodeIdAndContractor.second->serializedSize();
    }
    //----------------------------------------------------
    memcpy(
        bytesBufferOffset,
        &mMaximalClaimingBlockNumber,
        sizeof(BlockNumber));
    bytesBufferOffset += sizeof(BlockNumber);
    //----------------------------------------------------
    memcpy(
        bytesBufferOffset,
        &mIsReceiptContains,
        sizeof(byte_t));
    //----------------------------------------------------
    if (mIsReceiptContains) {
        bytesBufferOffset += sizeof(byte_t);
        memcpy(
            bytesBufferOffset,
            mSignature->data(),
            sphincs::Signature::signatureSize());
    }
    //----------------------------------------------------
    return make_pair(
               buffer,
               bytesCount);
}
