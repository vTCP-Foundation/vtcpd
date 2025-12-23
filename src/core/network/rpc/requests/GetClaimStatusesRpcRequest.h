#ifndef VTCPD_GETCLAIMSTATUSESRPCREQUEST_H
#define VTCPD_GETCLAIMSTATUSESRPCREQUEST_H

#include "../RpcRequest.h"
#include "../../../common/Types.h"
#include "../../../crypto/sphincskeys.h"
#include "../../../crypto/sphincsscheme.h"
#include "../../../../libs/json/json.h"
#include <vector>

using namespace crypto;
using json = nlohmann::json;


class GetClaimStatusesRpcRequest : public RpcRequest
{
public:
    using Shared = shared_ptr<GetClaimStatusesRpcRequest>;

    struct ClaimInfo {
        TransactionUUID transactionUUID;
        BlockNumber maxClaimBlockNumber;
    };

public:
    /**
     * Holds batch of claims to query statuses for.
     */
    GetClaimStatusesRpcRequest(
        const TransactionUUID &transactionUUID,
        const vector<ClaimInfo> &claims);

    ~GetClaimStatusesRpcRequest() override = default;

    /**
     * Identifies the RPC method for this request.
     */
    RpcMethod method() const override;

    /**
     * Provides immutable access to requested claims.
     */
    const vector<ClaimInfo>& claims() const;

    /**
     * Task 15-01: Returns public key used to authenticate claim status request.
     */
    sphincs::PublicKey::Shared publicKey() const;

    /**
     * Task 15-01: Returns signature covering serialized claim status payload.
     */
    sphincs::Signature::Shared signature() const;

    /**
     * Task 15-01: Sets public key used to authenticate claim status request.
     */
    void setPublicKey(sphincs::PublicKey::Shared publicKey);

    /**
     * Task 15-01: Sets signature covering serialized claim status payload.
     */
    void setSignature(sphincs::Signature::Shared signature);

    /**
     * Serializes claims into JSON-RPC payload.
     */
    json toJson() const;

private:
    /**
     * Task 15-01: Public key for observer request verification.
     */
    sphincs::PublicKey::Shared mPublicKey;

    /**
     * Task 15-01: Signature for observer request verification.
     */
    sphincs::Signature::Shared mSignature;

    vector<ClaimInfo> mClaims;
};

#endif // VTCPD_GETCLAIMSTATUSESRPCREQUEST_H
