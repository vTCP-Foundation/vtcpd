#ifndef VTCPD_GETCLAIMSTATUSESRPCREQUEST_H
#define VTCPD_GETCLAIMSTATUSESRPCREQUEST_H

#include "../RpcRequest.h"
#include "../../../common/Types.h"
#include "../../../../libs/json/json.h"
#include <vector>

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
     * Serializes claims into JSON-RPC payload.
     */
    json toJson() const;

private:
    vector<ClaimInfo> mClaims;
};

#endif // VTCPD_GETCLAIMSTATUSESRPCREQUEST_H
