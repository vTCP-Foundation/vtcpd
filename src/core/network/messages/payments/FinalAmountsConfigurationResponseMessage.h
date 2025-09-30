#ifndef VTCPD_FINALAMOUNTSCONFIGURATIONRESPONSEMESSAGE_H
#define VTCPD_FINALAMOUNTSCONFIGURATIONRESPONSEMESSAGE_H

#include "../base/transaction/TransactionMessage.h"
#include "../../../crypto/sphincsscheme.h"

using namespace crypto;

class FinalAmountsConfigurationResponseMessage : public TransactionMessage
{

public:
    enum OperationState
    {
        Accepted = 1,
        Rejected = 2,
    };

public:
    typedef shared_ptr<FinalAmountsConfigurationResponseMessage> Shared;

public:
    FinalAmountsConfigurationResponseMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> &senderAddresses,
        const TransactionUUID &transactionUUID,
        const OperationState state);

    FinalAmountsConfigurationResponseMessage(
        const SerializedEquivalent equivalent,
        vector<BaseAddress::Shared> &senderAddresses,
        const TransactionUUID &transactionUUID,
        const OperationState state,
        const sphincs::PublicKey::Shared publicKey);

    FinalAmountsConfigurationResponseMessage(
        BytesShared buffer);

    const MessageType typeID() const override;

    const OperationState state() const;

    const sphincs::PublicKey::Shared publicKey() const;

protected:
    typedef byte_t SerializedOperationState;

    pair<BytesShared, size_t> serializeToBytes() const override;

private:
    OperationState mState;
    sphincs::PublicKey::Shared mPublicKey;
};

#endif // VTCPD_FINALAMOUNTSCONFIGURATIONRESPONSEMESSAGE_H
