#ifndef VTCPD_FINALPATHCYCLECONFIGURATIONMESSAGE_H
#define VTCPD_FINALPATHCYCLECONFIGURATIONMESSAGE_H

#include "base/RequestCycleMessage.h"
#include "../../../contractors/Contractor.h"
#include "../../../crypto/sphincsscheme.h"
#include <map>

using namespace crypto;

class FinalPathCycleConfigurationMessage :
    public RequestCycleMessage
{

public:
    typedef shared_ptr<FinalPathCycleConfigurationMessage> Shared;

public:
    FinalPathCycleConfigurationMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> &senderAddresses,
        const TransactionUUID &transactionUUID,
        const TrustLineAmount &amount,
        const map<PaymentNodeID, Contractor::Shared> &paymentParticipants,
        const BlockNumber maximalClaimingBlockNumber);

    FinalPathCycleConfigurationMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> &senderAddresses,
        const TransactionUUID &transactionUUID,
        const TrustLineAmount &amount,
        const map<PaymentNodeID, Contractor::Shared> &paymentParticipants,
        const BlockNumber maximalClaimingBlockNumber,
        const sphincs::Signature::Shared signature,
        const sphincs::KeyHash::Shared transactionPublicKeyHash);

    FinalPathCycleConfigurationMessage(
        BytesShared buffer);

    const MessageType typeID() const override;

    const map<PaymentNodeID, Contractor::Shared>& paymentParticipants() const;

    const BlockNumber maximalClaimingBlockNumber() const;

    bool isReceiptContains() const;

    const sphincs::Signature::Shared signature() const;

    const sphincs::KeyHash::Shared transactionPublicKeyHash() const;

    pair<BytesShared, size_t> serializeToBytes() const override;

private:
    map<PaymentNodeID, Contractor::Shared> mPaymentParticipants;
    BlockNumber mMaximalClaimingBlockNumber;
    bool mIsReceiptContains;
    sphincs::Signature::Shared mSignature;
    sphincs::KeyHash::Shared mTransactionPublicKeyHash;
};


#endif //VTCPD_FINALPATHCYCLECONFIGURATIONMESSAGE_H
