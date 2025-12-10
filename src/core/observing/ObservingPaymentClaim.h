#ifndef VTCPD_OBSERVINGPAYMENTCLAIM_H
#define VTCPD_OBSERVINGPAYMENTCLAIM_H

#include "../common/Types.h"
#include "../transactions/transactions/base/TransactionUUID.h"
#include "../crypto/sphincskeys.h"
#include "../crypto/sphincsscheme.h"

#include <map>
#include <memory>

using namespace std;
using namespace crypto;

class ObservingPaymentClaim
{

public:
    typedef shared_ptr<ObservingPaymentClaim> Shared;

    enum ClaimStatus
    {
        NoInfo = 0,
        Observing = 1,
        ParticipantsVotesPresent = 2,
        RejectedByObserving = 3,
        Done = 4
    };

public:
    ObservingPaymentClaim(
        const TransactionUUID &transactionUUID,
        BlockNumber maxBlockNumberForClaiming,
        const map<PaymentNodeID, sphincs::PublicKey::Shared> &participantsPublicKeys,
        sphincs::PublicKey::Shared publicKey,
        sphincs::Signature::Shared signature);

    const TransactionUUID &transactionUUID() const;

    BlockNumber maxBlockNumberForClaiming() const;

    const map<PaymentNodeID, sphincs::PublicKey::Shared> &participantsPublicKeys() const;

    sphincs::PublicKey::Shared publicKey() const;

    sphincs::Signature::Shared signature() const;

    ClaimStatus status() const;

    void setStatus(ClaimStatus status);

private:
    TransactionUUID mTransactionUUID;
    BlockNumber mMaxBlockNumberForClaiming;
    map<PaymentNodeID, sphincs::PublicKey::Shared> mParticipantsPublicKeys;
    sphincs::PublicKey::Shared mPublicKey;
    sphincs::Signature::Shared mSignature;
    ClaimStatus mStatus;
};

#endif // VTCPD_OBSERVINGPAYMENTCLAIM_H
