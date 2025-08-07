#ifndef VTCPD_OBSERVINGPARTICIPANTSVOTESRESPONSEMESSAGE_H
#define VTCPD_OBSERVINGPARTICIPANTSVOTESRESPONSEMESSAGE_H

#include "base/ObservingResponseMessage.h"
#include "../../transactions/transactions/base/TransactionUUID.h"
#include "../../crypto/sphincsscheme.h"

#include <map>

using namespace crypto;

class ObservingParticipantsVotesResponseMessage : public ObservingResponseMessage
{

public:
    typedef shared_ptr<ObservingParticipantsVotesResponseMessage> Shared;

public:
    ObservingParticipantsVotesResponseMessage(
        BytesShared buffer);

    bool isParticipantsVotesPresent() const;

    const TransactionUUID& transactionUUID() const;

    const BlockNumber maximalClaimingBlockNumber() const;

    const map<PaymentNodeID, sphincs::Signature::Shared>& participantsSignatures() const;

private:
    ObservingTransaction::SerializedObservingResponseType mObservingResponse;
    bool mIsParticipantsVotesPresent;
    TransactionUUID mTransactionUUID;
    BlockNumber mMaximalClaimingBlockNumber;
    map<PaymentNodeID, sphincs::Signature::Shared> mParticipantsSignatures;
};


#endif //VTCPD_OBSERVINGPARTICIPANTSVOTESRESPONSEMESSAGE_H
