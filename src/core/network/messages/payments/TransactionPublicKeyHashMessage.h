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
    TransactionPublicKeyHashMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> &senderAddresses,
        const TransactionUUID &transactionUUID,
        const PaymentNodeID paymentNodeID,
        const sphincs::KeyHash::Shared transactionPublicKeyHash);

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

    const sphincs::Signature::Shared signature() const;

    pair<BytesShared, size_t> serializeToBytes() const override;

private:
    PaymentNodeID mPaymentNodeID;
    sphincs::KeyHash::Shared mTransactionPublicKeyHash;
    bool mIsReceiptContains;
    sphincs::Signature::Shared mSignature;
};


#endif //VTCPD_TRANSACTIONPUBLICKEYHASHMESSAGE_H
