#ifndef VTCPD_OBSERVINGREQUESTMESSAGE_H
#define VTCPD_OBSERVINGREQUESTMESSAGE_H

#include "base/ObservingMessage.hpp"
#include "../../transactions/transactions/base/TransactionUUID.h"
#include "../../crypto/sphincsscheme.h"

#include <map>

using namespace crypto;

class ObservingClaimAppendRequestMessage : public ObservingMessage
{

public:
    typedef shared_ptr<ObservingClaimAppendRequestMessage> Shared;

public:
    ObservingClaimAppendRequestMessage(
        const TransactionUUID& transactionUUID,
        BlockNumber maximalClaimingBlockNumber,
        const map<PaymentNodeID, sphincs::PublicKey::Shared>& participantsPublicKeys);

    const TransactionUUID& transactionUUID() const;

    const BlockNumber maximalClaimingBlockNumber() const;

    const MessageType typeID() const override;

    BytesShared serializeToBytes() const override;

    size_t serializedSize() const override;

private:
    TransactionUUID mTransactionUUID;
    BlockNumber mMaximalClaimingBlockNumber;
    map<PaymentNodeID, sphincs::PublicKey::Shared> mParticipantsPublicKeys;
};


#endif //VTCPD_OBSERVINGREQUESTMESSAGE_H
