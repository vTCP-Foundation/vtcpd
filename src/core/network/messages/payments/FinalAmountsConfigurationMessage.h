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
    // Constructor without receipts
    FinalAmountsConfigurationMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> senderAddresses,
        const TransactionUUID &transactionUUID,
        const vector<pair<PathID, ConstSharedTrustLineAmount>> &finalAmountsConfig,
        const map<PaymentNodeID, Contractor::Shared> &paymentParticipants,
        const BlockNumber maximalClaimingBlockNumber);

    // NEW: Constructor with multiple receipts (for new transactions)
    FinalAmountsConfigurationMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> senderAddresses,
        const TransactionUUID &transactionUUID,
        const vector<pair<PathID, ConstSharedTrustLineAmount>> &finalAmountsConfig,
        const map<PaymentNodeID, Contractor::Shared> &paymentParticipants,
        const BlockNumber maximalClaimingBlockNumber,
        const vector<pair<SerializedEquivalent, sphincs::Signature::Shared>> &signatures);

    // DEPRECATED: Constructor with single receipt (for old transactions backward compatibility)
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

    const vector<pair<SerializedEquivalent, sphincs::Signature::Shared>>& signatures() const;

    pair<BytesShared, size_t> serializeToBytes() const override;

private:
    map<PaymentNodeID, Contractor::Shared> mPaymentParticipants;
    BlockNumber mMaximalClaimingBlockNumber;
    vector<pair<SerializedEquivalent, sphincs::Signature::Shared>> mSignatures;
};


#endif //VTCPD_FINALAMOUNTSCONFIGURATIONMESSAGE_H
