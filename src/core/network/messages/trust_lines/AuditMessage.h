#ifndef VTCPD_AUDITMESSAGE_H
#define VTCPD_AUDITMESSAGE_H

#include "../base/transaction/TransactionMessage.h"
#include "../../../crypto/sphincsscheme.h"
#include "../../../common/multiprecision/MultiprecisionUtils.h"
#include <vector>

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
        const sphincs::Signature::Shared signature,
        const vector<TransactionUUID> &transactionUUIDs = {});

    AuditMessage(
        BytesShared buffer);

    const AuditNumber auditNumber() const;

    const TrustLineAmount& incomingAmount() const;

    const TrustLineAmount& outgoingAmount() const;

    // Returns the audit transaction UUID list.
    const vector<TransactionUUID>& transactionUUIDs() const;

    const sphincs::Signature::Shared signature() const;

    // Sorts UUIDs lexicographically by raw bytes for deterministic ordering.
    static void sortTransactionUUIDs(
        vector<TransactionUUID> &transactionUUIDs);

    // Returns a sorted, unique copy of the UUID list.
    static vector<TransactionUUID> normalizedTransactionUUIDs(
        const vector<TransactionUUID> &transactionUUIDs);

    // Computes SHA-256 over count + concatenated UUID bytes.
    static BytesShared computeTransactionListHash(
        const vector<TransactionUUID> &transactionUUIDs);

    // Defines the hash size for transaction UUID lists.
    static constexpr size_t kTransactionUUIDsHashSize = 32;

    const MessageType typeID() const override;

    const bool isCheckCachedResponse() const override;

    pair<BytesShared, size_t> serializeToBytes() const override;

protected:
    const size_t kOffsetToInheritedBytes() const override;

private:
    AuditNumber mAuditNumber;
    TrustLineAmount mIncomingAmount;
    TrustLineAmount mOutgoingAmount;
    // Stores the audit transaction UUID list in canonical order.
    vector<TransactionUUID> mTransactionUUIDs;
    sphincs::Signature::Shared mSignature;
};


#endif //VTCPD_AUDITMESSAGE_H
