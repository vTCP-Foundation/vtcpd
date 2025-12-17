#ifndef VTCPD_SUBMITCLAIMVOTESRPCRESPONSE_H
#define VTCPD_SUBMITCLAIMVOTESRPCRESPONSE_H

#include "../RpcResponse.h"
#include "../../../../libs/json/json.h"

using json = nlohmann::json;


class SubmitClaimVotesRpcResponse : public RpcResponse
{
public:
    using Shared = shared_ptr<SubmitClaimVotesRpcResponse>;

public:
    /**
     * Holds success flag and message for SubmitClaimVotes RPC.
     */
    SubmitClaimVotesRpcResponse(
        const TransactionUUID &transactionUUID,
        RpcResponseStatus status,
        bool success = false,
        const string &message = "",
        const string &errorMessage = "");

    ~SubmitClaimVotesRpcResponse() override = default;

    /**
     * Identifies originating RPC method.
     */
    RpcMethod method() const override;

    /**
     * Returns observer-reported success flag.
     */
    bool success() const;

    /**
     * Returns observer message (both success and failure cases).
     */
    const string& message() const;

    /**
     * Parses JSON payload into typed response.
     */
    static Shared fromJson(
        const TransactionUUID &transactionUUID,
        RpcResponseStatus status,
        const json &responseJson,
        const string &errorMessage = "");

private:
    bool mSuccess;
    string mMessage;
};

#endif // VTCPD_SUBMITCLAIMVOTESRPCRESPONSE_H
