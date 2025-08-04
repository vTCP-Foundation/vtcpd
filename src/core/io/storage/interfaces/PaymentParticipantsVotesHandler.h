#ifndef VTCPD_INTERFACES_PAYMENTPARTICIPANTSVOTESHANDLER_H
#define VTCPD_INTERFACES_PAYMENTPARTICIPANTSVOTESHANDLER_H

#include "../../../logger/Logger.h"
#include "../../../common/exceptions/IOError.h"
#include "../../../common/exceptions/NotFoundError.h"
#include "../../../common/exceptions/ValueError.h"
#include "../../../transactions/transactions/base/TransactionUUID.h"
#include "../../../common/Types.h"
#include "../../../contractors/Contractor.h"
#include "../../../crypto/lamportkeys.h"
#include "../../../crypto/lamportscheme.h"

using namespace crypto::lamport;

class PaymentParticipantsVotesHandler
{
public:
    virtual ~PaymentParticipantsVotesHandler() = default;

    virtual void saveRecord(
        const TransactionUUID &transactionUUID,
        Contractor::Shared contractor,
        const PaymentNodeID paymentNodeID,
        const PublicKey::Shared publicKey,
        const Signature::Shared signature) = 0;

    virtual map<PaymentNodeID, Signature::Shared> participantsSignatures(
        const TransactionUUID &transactionUUID) = 0;

    virtual void deleteRecords(
        const TransactionUUID &transactionUUID) = 0;
};

#endif //VTCPD_INTERFACES_PAYMENTPARTICIPANTSVOTESHANDLER_H 