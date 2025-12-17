#ifndef VTCPD_GETCLAIMSTATUSESRPCRESPONSE_H
#define VTCPD_GETCLAIMSTATUSESRPCRESPONSE_H

#include "../RpcResponse.h"
#include "GetClaimStatusRpcResponse.h"
#include "../../../../libs/json/json.h"
#include <vector>

using json = nlohmann::json;


class GetClaimStatusesRpcResponse : public RpcResponse
{
public:
    using Shared = shared_ptr<GetClaimStatusesRpcResponse>;
    using ClaimState = GetClaimStatusRpcResponse::ClaimState;

    struct ClaimStatus {
        TransactionUUID transactionUUID;
        BlockNumber maxClaimBlockNumber;
        ClaimState state;
    };

public:
    /**
     * Captures batch claim statuses returned by observer.
     */
    GetClaimStatusesRpcResponse(
        const TransactionUUID &transactionUUID,
        RpcResponseStatus status,
        const vector<ClaimStatus> &statuses = {},
        const string &errorMessage = "");

    ~GetClaimStatusesRpcResponse() override = default;

    /**
     * Identifies RPC method associated with this response.
     */
    RpcMethod method() const override;

    /**
     * Provides list of statuses parsed from observer response.
     */
    const vector<ClaimStatus>& statuses() const;

    /**
     * Parses JSON payload into typed response; parse failures yield ParseError.
     */
    static Shared fromJson(
        const TransactionUUID &transactionUUID,
        RpcResponseStatus status,
        const json &responseJson,
        const string &errorMessage = "");

private:
    vector<ClaimStatus> mStatuses;
};

#endif // VTCPD_GETCLAIMSTATUSESRPCRESPONSE_H
