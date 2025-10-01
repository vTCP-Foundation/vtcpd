#ifndef VTCPD_RECEIPTRECORD_H
#define VTCPD_RECEIPTRECORD_H

#include "../../../../common/Types.h"
#include "../../../../crypto/sphincskeys.h"
#include "../../../../crypto/sphincsscheme.h"
#include "../../../../common/multiprecision/MultiprecisionUtils.h"
#include "../../../../common/memory/MemoryUtils.h"
#include "../../../../transactions/transactions/base/TransactionUUID.h"

using namespace crypto;

class ReceiptRecord
{

public:
    typedef shared_ptr<ReceiptRecord> Shared;

public:
    ReceiptRecord(
        const AuditNumber auditNumber,
        const TransactionUUID &transactionUUID,
        const TrustLineAmount &amount,
        const sphincs::KeyHash::Shared keyHash,
        const sphincs::Signature::Shared);

    ReceiptRecord(
        byte_t* buffer);

    const AuditNumber auditNumber() const;

    const TransactionUUID &transactionUUID() const;

    const TrustLineAmount &amount() const;

    const sphincs::KeyHash::Shared keyHash() const;

    const sphincs::Signature::Shared signature() const;

    BytesShared serializeToBytes();

    static const size_t recordSize();

    friend bool operator==(
        const ReceiptRecord::Shared record1,
        const ReceiptRecord::Shared record2);

private:
    AuditNumber mAuditNumber;
    TransactionUUID mTransactionUUID;
    TrustLineAmount mAmount;
    sphincs::KeyHash::Shared mKeyHash;
    sphincs::Signature::Shared mSignature;
};

#endif // VTCPD_RECEIPTRECORD_H
