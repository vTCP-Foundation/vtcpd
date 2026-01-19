#ifndef VTCPD_AUDITRESPONSEMESSAGE_H
#define VTCPD_AUDITRESPONSEMESSAGE_H

#include "../base/transaction/ConfirmationMessage.h"
#include "../../../crypto/sphincsscheme.h"
#include <vector>

using namespace crypto;

class AuditResponseMessage : public ConfirmationMessage
{

public:
    typedef shared_ptr<AuditResponseMessage> Shared;

public:
    AuditResponseMessage(
        const SerializedEquivalent equivalent,
        Contractor::Shared contractor,
        const TransactionUUID &transactionUUID,
        const sphincs::Signature::Shared signature,
        const vector<TransactionUUID> &transactionUUIDs = {});

    AuditResponseMessage(
        const SerializedEquivalent equivalent,
        Contractor::Shared contractor,
        const TransactionUUID &transactionUUID,
        OperationState state,
        const vector<TransactionUUID> &transactionUUIDs = {});

    AuditResponseMessage(
        BytesShared buffer);

    // Returns the audit transaction UUID list.
    const vector<TransactionUUID>& transactionUUIDs() const;

    const sphincs::Signature::Shared signature() const;

    const MessageType typeID() const override;

    pair<BytesShared, size_t> serializeToBytes() const override;

private:
    // Stores the audit transaction UUID list in canonical order.
    vector<TransactionUUID> mTransactionUUIDs;
    sphincs::Signature::Shared mSignature;
};


#endif //VTCPD_AUDITRESPONSEMESSAGE_H
