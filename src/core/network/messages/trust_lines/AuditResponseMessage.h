#ifndef VTCPD_AUDITRESPONSEMESSAGE_H
#define VTCPD_AUDITRESPONSEMESSAGE_H

#include "../base/transaction/ConfirmationMessage.h"
#include "../../../crypto/sphincsscheme.h"

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
        const sphincs::Signature::Shared signature);

    AuditResponseMessage(
        const SerializedEquivalent equivalent,
        Contractor::Shared contractor,
        const TransactionUUID &transactionUUID,
        OperationState state);

    AuditResponseMessage(
        BytesShared buffer);

    const sphincs::Signature::Shared signature() const;

    const MessageType typeID() const override;

    pair<BytesShared, size_t> serializeToBytes() const override;

private:
    sphincs::Signature::Shared mSignature;
};


#endif //VTCPD_AUDITRESPONSEMESSAGE_H
