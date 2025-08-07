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
        sphincs::KeyHash::Shared ownKeyHash,
        sphincs::Signature::Shared ownSignature,
        sphincs::KeyHash::Shared contractorKeyHash,
        sphincs::Signature::Shared contractorSignature,
        sphincs::KeyHash::Shared ownKeysSetHash,
        sphincs::KeyHash::Shared contractorKeysSetHash);

    AuditRecord(
        byte_t* buffer);

    const AuditNumber auditNumber() const;

    const TrustLineAmount &incomingAmount() const;

    const TrustLineAmount &outgoingAmount() const;

    const TrustLineBalance &balance() const;

    const sphincs::KeyHash::Shared ownKeyHash() const;

    const sphincs::Signature::Shared ownSignature() const;

    const sphincs::KeyHash::Shared contractorKeyHash() const;

    const sphincs::Signature::Shared contractorSignature() const;

    const sphincs::KeyHash::Shared ownKeysSetHash() const;

    const sphincs::KeyHash::Shared contractorKeysSetHash() const;

    void setContractorSignature(
        sphincs::Signature::Shared signature);

    bool isPendingState() const;

    void setOwnKeysSetHash(
        sphincs::KeyHash::Shared ownKeysSetHash);

    void setContractorKeysSetHash(
        sphincs::KeyHash::Shared contractorKeysSetHash);

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
    sphincs::KeyHash::Shared mOwnKeyHash;
    sphincs::Signature::Shared mOwnSignature;
    sphincs::KeyHash::Shared mContractorKeyHash;
    sphincs::Signature::Shared mContractorSignature;
    sphincs::KeyHash::Shared mOwnKeysSetHash;
    sphincs::KeyHash::Shared mContractorKeysSetHash;
};

#endif // VTCPD_AUDITRECORD_H
