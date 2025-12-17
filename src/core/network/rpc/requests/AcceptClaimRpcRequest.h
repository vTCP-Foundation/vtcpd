#ifndef VTCPD_ACCEPTCLAIMRPCREQUEST_H
#define VTCPD_ACCEPTCLAIMRPCREQUEST_H

#include "../RpcRequest.h"
#include "../../../observing/ObservingPaymentClaim.h"
#include "../../../../libs/json/json.h"
#include <map>

using json = nlohmann::json;


class AcceptClaimRpcRequest : public RpcRequest
{
public:
    using Shared = shared_ptr<AcceptClaimRpcRequest>;

public:
    /**
     * Holds all parameters required by observer to register a claim.
     */
    AcceptClaimRpcRequest(
        const TransactionUUID &transactionUUID,
        const TransactionUUID &claimTransactionUUID,
        BlockNumber maxClaimBlockNumber,
        const map<PaymentNodeID, sphincs::PublicKey::Shared> &participantsPublicKeys,
        sphincs::PublicKey::Shared publicKey,
        sphincs::Signature::Shared signature);

    ~AcceptClaimRpcRequest() override = default;

    /**
     * Identifies this request as AcceptClaim RPC.
     */
    RpcMethod method() const override;

    /**
     * Exposes the claim transaction UUID that observer should register.
     */
    const TransactionUUID& claimTransactionUUID() const;

    /**
     * Provides maximal block number allowed for the claim.
     */
    BlockNumber maxClaimBlockNumber() const;

    /**
     * Returns participants public keys ordered by PaymentNodeID.
     */
    const map<PaymentNodeID, sphincs::PublicKey::Shared>& participantsPublicKeys() const;

    /**
     * Returns submitter public key used for verification on observer side.
     */
    sphincs::PublicKey::Shared publicKey() const;

    /**
     * Returns submitter signature covering claim payload.
     */
    sphincs::Signature::Shared signature() const;

    /**
     * Serializes parameters into JSON-RPC payload per observer contract.
     */
    json toJson() const;

private:
    TransactionUUID mClaimTransactionUUID;
    BlockNumber mMaxClaimBlockNumber;
    map<PaymentNodeID, sphincs::PublicKey::Shared> mParticipantsPublicKeys;
    sphincs::PublicKey::Shared mPublicKey;
    sphincs::Signature::Shared mSignature;
};

#endif // VTCPD_ACCEPTCLAIMRPCREQUEST_H
