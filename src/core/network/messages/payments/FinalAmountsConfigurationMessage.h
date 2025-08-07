#ifndef VTCPD_FINALAMOUNTSCONFIGURATIONMESSAGE_H
#define VTCPD_FINALAMOUNTSCONFIGURATIONMESSAGE_H

#include "base/RequestMessageWithReservations.h"
#include "../../../contractors/Contractor.h"
#include "../../../crypto/sphincsscheme.h"
#include <map>

using namespace crypto;

class FinalAmountsConfigurationMessage : public RequestMessageWithReservations
{

public:
    typedef shared_ptr<FinalAmountsConfigurationMessage> Shared;

public:
    FinalAmountsConfigurationMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> senderAddresses,
        const TransactionUUID &transactionUUID,
        const vector<pair<PathID, ConstSharedTrustLineAmount>> &finalAmountsConfig,
        const map<PaymentNodeID, Contractor::Shared> &paymentParticipants,
        const BlockNumber maximalClaimingBlockNumber);

    // if coordinator has reservation with current node it also send receipt
    FinalAmountsConfigurationMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> senderAddresses,
        const TransactionUUID &transactionUUID,
        const vector<pair<PathID, ConstSharedTrustLineAmount>> &finalAmountsConfig,
        const map<PaymentNodeID, Contractor::Shared> &mPaymentParticipants,
        const BlockNumber maximalClaimingBlockNumber,
        const sphincs::Signature::Shared signature);

    FinalAmountsConfigurationMessage(
        BytesShared buffer);

    const MessageType typeID() const override;

    const map<PaymentNodeID, Contractor::Shared> &paymentParticipants() const;

    const BlockNumber maximalClaimingBlockNumber() const;

    bool isReceiptContains() const;

    const sphincs::Signature::Shared signature() const;

    pair<BytesShared, size_t> serializeToBytes() const override;

private:
    map<PaymentNodeID, Contractor::Shared> mPaymentParticipants;
    BlockNumber mMaximalClaimingBlockNumber;
    bool mIsReceiptContains;
    sphincs::Signature::Shared mSignature;
};


#endif //VTCPD_FINALAMOUNTSCONFIGURATIONMESSAGE_H
