#ifndef VTCPD_AUDITRECORD_H
#define VTCPD_AUDITRECORD_H

#include "../../../../common/Types.h"
#include "../../../../crypto/sphincskeys.h"
#include "../../../../crypto/sphincsscheme.h"
#include "../../../../common/multiprecision/MultiprecisionUtils.h"
#include "../../../../common/memory/MemoryUtils.h"

using namespace crypto;

class AuditRecord
{
public:
    typedef shared_ptr<AuditRecord> Shared;

public:
    AuditRecord(
        AuditNumber auditNumber,
        TrustLineAmount &incomingAmount,
        TrustLineAmount &outgoingAmount,
        TrustLineBalance &balance);

    AuditRecord(
        AuditNumber auditNumber,
        TrustLineAmount &incomingAmount,
        TrustLineAmount &outgoingAmount,
        TrustLineBalance &balance,
        sphincs::Signature::Shared ownSignature,
        sphincs::Signature::Shared contractorSignature);

    AuditRecord(
        byte_t* buffer);

    const AuditNumber auditNumber() const;

    const TrustLineAmount &incomingAmount() const;

    const TrustLineAmount &outgoingAmount() const;

    const TrustLineBalance &balance() const;

    const sphincs::Signature::Shared ownSignature() const;

    const sphincs::Signature::Shared contractorSignature() const;

    void setContractorSignature(
        sphincs::Signature::Shared signature);

    bool isPendingState() const;


    BytesShared serializeToBytes();

    BytesShared serializeToCheckSignatureByInitiator();

    BytesShared serializeToCheckSignatureByContractor();

    static const size_t recordSize();

    static const size_t recordSizeForSignatureChecking();

private:
    AuditNumber mAuditNumber;
    TrustLineAmount mIncomingAmount;
    TrustLineAmount mOutgoingAmount;
    TrustLineBalance mBalance;
    sphincs::Signature::Shared mOwnSignature;
    sphincs::Signature::Shared mContractorSignature;
};

#endif // VTCPD_AUDITRECORD_H
