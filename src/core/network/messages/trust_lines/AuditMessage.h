#ifndef VTCPD_AUDITMESSAGE_H
#define VTCPD_AUDITMESSAGE_H

#include "../base/transaction/TransactionMessage.h"
#include "../../../crypto/sphincsscheme.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"

using namespace crypto;

class AuditMessage : public TransactionMessage
{

public:
    typedef shared_ptr<AuditMessage> Shared;

public:
    AuditMessage(
        const SerializedEquivalent equivalent,
        Contractor::Shared contractor,
        const TransactionUUID &transactionUUID,
        const AuditNumber auditNumber,
        const TrustLineAmount &incomingAmount,
        const TrustLineAmount &outgoingAmount,
        const sphincs::Signature::Shared signature);

    AuditMessage(
        BytesShared buffer);

    const AuditNumber auditNumber() const;

    const TrustLineAmount& incomingAmount() const;

    const TrustLineAmount& outgoingAmount() const;

    const sphincs::Signature::Shared signature() const;

    const MessageType typeID() const override;

    const bool isCheckCachedResponse() const override;

    pair<BytesShared, size_t> serializeToBytes() const override;

protected:
    const size_t kOffsetToInheritedBytes() const override;

private:
    AuditNumber mAuditNumber;
    TrustLineAmount mIncomingAmount;
    TrustLineAmount mOutgoingAmount;
    sphincs::Signature::Shared mSignature;
};


#endif //VTCPD_AUDITMESSAGE_H
