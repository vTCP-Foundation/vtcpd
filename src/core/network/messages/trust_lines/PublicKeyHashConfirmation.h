#ifndef VTCPD_PUBLICKEYHASHCONFIRMATION_H
#define VTCPD_PUBLICKEYHASHCONFIRMATION_H

#include "../base/transaction/ConfirmationMessage.h"
#include "../../../crypto/sphincsscheme.h"

using namespace crypto;

class PublicKeyHashConfirmation : public ConfirmationMessage
{

public:
    typedef shared_ptr<PublicKeyHashConfirmation> Shared;

public:
    PublicKeyHashConfirmation(
        const SerializedEquivalent equivalent,
        Contractor::Shared contractor,
        const TransactionUUID &transactionUUID,
        sphincs::KeyHash::Shared hashConfirmation);

    PublicKeyHashConfirmation(
        const SerializedEquivalent equivalent,
        Contractor::Shared contractor,
        const TransactionUUID &transactionUUID,
        OperationState state);

    PublicKeyHashConfirmation(
        BytesShared buffer);

    const sphincs::KeyHash::Shared hashConfirmation() const;

    const MessageType typeID() const override;

    pair<BytesShared, size_t> serializeToBytes() const override;

private:
    sphincs::KeyHash::Shared mHashConfirmation;
};


#endif //VTCPD_PUBLICKEYHASHCONFIRMATION_H
