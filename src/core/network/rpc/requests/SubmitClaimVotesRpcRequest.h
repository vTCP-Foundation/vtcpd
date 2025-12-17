#ifndef VTCPD_SUBMITCLAIMVOTESRPCREQUEST_H
#define VTCPD_SUBMITCLAIMVOTESRPCREQUEST_H

#include "../RpcRequest.h"
#include "../../../common/Types.h"
#include "../../../crypto/sphincsscheme.h"
#include "../../../../libs/json/json.h"
#include <map>

using namespace crypto;
using json = nlohmann::json;


class SubmitClaimVotesRpcRequest : public RpcRequest
{
public:
    using Shared = shared_ptr<SubmitClaimVotesRpcRequest>;

public:
    /**
     * Contains votes payload and verification data to submit to observer.
     */
    SubmitClaimVotesRpcRequest(
        const TransactionUUID &transactionUUID,
        const TransactionUUID &claimTransactionUUID,
        BlockNumber maxClaimBlockNumber,
        const map<PaymentNodeID, sphincs::Signature::Shared> &votes,
        sphincs::PublicKey::Shared publicKey,
        sphincs::Signature::Shared signature);

    ~SubmitClaimVotesRpcRequest() override = default;

    /**
     * Identifies this request as SubmitClaimVotes RPC.
     */
    RpcMethod method() const override;

    /**
     * Returns claim UUID for which votes are submitted.
     */
    const TransactionUUID& claimTransactionUUID() const;

    /**
     * Returns maximal claim block number for validation on observer side.
     */
    BlockNumber maxClaimBlockNumber() const;

    /**
     * Provides vote signatures keyed by PaymentNodeID.
     */
    const map<PaymentNodeID, sphincs::Signature::Shared>& votes() const;

    /**
     * Returns submitter public key used for verification.
     */
    sphincs::PublicKey::Shared publicKey() const;

    /**
     * Returns submitter signature covering vote submission.
     */
    sphincs::Signature::Shared signature() const;

    /**
     * Serializes the request per observer JSON-RPC contract.
     */
    json toJson() const;

private:
    TransactionUUID mClaimTransactionUUID;
    BlockNumber mMaxClaimBlockNumber;
    map<PaymentNodeID, sphincs::Signature::Shared> mVotes;
    sphincs::PublicKey::Shared mPublicKey;
    sphincs::Signature::Shared mSignature;
};

#endif // VTCPD_SUBMITCLAIMVOTESRPCREQUEST_H
