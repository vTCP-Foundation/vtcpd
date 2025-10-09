#ifndef VTCPD_TRANSACTIONPUBLICKEYHASHMESSAGE_H
#define VTCPD_TRANSACTIONPUBLICKEYHASHMESSAGE_H

#include "../base/transaction/TransactionMessage.h"
#include "../../../crypto/sphincsscheme.h"

using namespace crypto;

class TransactionPublicKeyHashMessage : public TransactionMessage
{

public:
    typedef shared_ptr<TransactionPublicKeyHashMessage> Shared;

public:
    // Constructor without receipts
    TransactionPublicKeyHashMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> &senderAddresses,
        const TransactionUUID &transactionUUID,
        const PaymentNodeID paymentNodeID,
        const sphincs::KeyHash::Shared transactionPublicKeyHash);

    // NEW: Constructor with multiple receipts (for new transactions)
    TransactionPublicKeyHashMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> &senderAddresses,
        const TransactionUUID &transactionUUID,
        const PaymentNodeID paymentNodeID,
        const sphincs::KeyHash::Shared transactionPublicKeyHash,
        const vector<pair<SerializedEquivalent, sphincs::Signature::Shared>> &signatures);

    // DEPRECATED: Constructor with single receipt (for old transactions backward compatibility)
    TransactionPublicKeyHashMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> &senderAddresses,
        const TransactionUUID &transactionUUID,
        const PaymentNodeID paymentNodeID,
        const sphincs::KeyHash::Shared transactionPublicKeyHash,
        const sphincs::Signature::Shared signature);

    TransactionPublicKeyHashMessage(
        BytesShared buffer);

    const MessageType typeID() const override;

    const PaymentNodeID paymentNodeID() const;

    const sphincs::KeyHash::Shared transactionPublicKeyHash() const;

    bool isReceiptContains() const;

    const vector<pair<SerializedEquivalent, sphincs::Signature::Shared>>& signatures() const;

    pair<BytesShared, size_t> serializeToBytes() const override;

private:
    PaymentNodeID mPaymentNodeID;
    sphincs::KeyHash::Shared mTransactionPublicKeyHash;
    vector<pair<SerializedEquivalent, sphincs::Signature::Shared>> mSignatures;
};


#endif //VTCPD_TRANSACTIONPUBLICKEYHASHMESSAGE_H
