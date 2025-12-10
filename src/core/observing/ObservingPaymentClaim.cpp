#include "ObservingPaymentClaim.h"

ObservingPaymentClaim::ObservingPaymentClaim(
    const TransactionUUID &transactionUUID,
    BlockNumber maxBlockNumberForClaiming,
    const map<PaymentNodeID, sphincs::PublicKey::Shared> &participantsPublicKeys,
    sphincs::PublicKey::Shared publicKey,
    sphincs::Signature::Shared signature) :
    mTransactionUUID(transactionUUID),
    mMaxBlockNumberForClaiming(maxBlockNumberForClaiming),
    mParticipantsPublicKeys(participantsPublicKeys),
    mPublicKey(publicKey),
    mSignature(signature),
    mStatus(NoInfo)
{}

const TransactionUUID &ObservingPaymentClaim::transactionUUID() const
{
    return mTransactionUUID;
}

BlockNumber ObservingPaymentClaim::maxBlockNumberForClaiming() const
{
    return mMaxBlockNumberForClaiming;
}

const map<PaymentNodeID, sphincs::PublicKey::Shared> &ObservingPaymentClaim::participantsPublicKeys() const
{
    return mParticipantsPublicKeys;
}

sphincs::PublicKey::Shared ObservingPaymentClaim::publicKey() const
{
    return mPublicKey;
}

sphincs::Signature::Shared ObservingPaymentClaim::signature() const
{
    return mSignature;
}

ObservingPaymentClaim::ClaimStatus ObservingPaymentClaim::status() const
{
    return mStatus;
}

void ObservingPaymentClaim::setStatus(
    ClaimStatus status)
{
    mStatus = status;
}
