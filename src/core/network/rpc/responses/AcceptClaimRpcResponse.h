#ifndef VTCPD_ACCEPTCLAIMRPCRESPONSE_H
#define VTCPD_ACCEPTCLAIMRPCRESPONSE_H

#include "../RpcResponse.h"
#include "../../../../libs/json/json.h"

using json = nlohmann::json;


class AcceptClaimRpcResponse : public RpcResponse
{
public:
    using Shared = shared_ptr<AcceptClaimRpcResponse>;

public:
    /**
     * Holds success flag and observer message for AcceptClaim.
     */
    AcceptClaimRpcResponse(
        const TransactionUUID &transactionUUID,
        RpcResponseStatus status,
        bool success = false,
        const string &message = "",
        const string &errorMessage = "");

    ~AcceptClaimRpcResponse() override = default;

    /**
     * Identifies method used to obtain this response.
     */
    RpcMethod method() const override;

    /**
     * Indicates whether observer accepted the claim.
     */
    bool success() const;

    /**
     * Provides message returned by observer (both success and failure).
     */
    const string& message() const;

    /**
     * Parses JSON payload into typed response; parser errors mapped to ParseError.
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

#endif // VTCPD_ACCEPTCLAIMRPCRESPONSE_H
