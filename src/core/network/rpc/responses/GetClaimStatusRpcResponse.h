#ifndef VTCPD_GETCLAIMSTATUSRPCRESPONSE_H
#define VTCPD_GETCLAIMSTATUSRPCRESPONSE_H

#include "../RpcResponse.h"
#include "../../../common/Types.h"
#include "../../../crypto/sphincsscheme.h"
#include "../../../../libs/json/json.h"
#include <map>

using namespace crypto;
using json = nlohmann::json;


class GetClaimStatusRpcResponse : public RpcResponse
{
public:
    using Shared = shared_ptr<GetClaimStatusRpcResponse>;

    enum class ClaimState {
        NotFound = 0,
        Observing = 1,
        Approved = 2,
        Rejected = 3,
    };

public:
    /**
     * Captures typed fields for claim status, votes, and rejection signature.
     */
    GetClaimStatusRpcResponse(
        const TransactionUUID &transactionUUID,
        RpcResponseStatus status,
        ClaimState state = ClaimState::NotFound,
        const map<PaymentNodeID, sphincs::Signature::Shared> &votes = {},
        const string &rejectionSignature = "",
        const string &errorMessage = "");

    ~GetClaimStatusRpcResponse() override = default;

    /**
     * Identifies owning RPC method.
     */
    RpcMethod method() const override;

    /**
     * Returns mapped claim state from observer response.
     */
    ClaimState state() const;

    /**
     * Provides votes map populated when state is Approved.
     */
    const map<PaymentNodeID, sphincs::Signature::Shared>& votes() const;

    /**
     * Returns rejection signature when state is Rejected.
     */
    const string& rejectionSignature() const;

    /**
     * Builds typed response from observer JSON payload.
     */
    static Shared fromJson(
        const TransactionUUID &transactionUUID,
        RpcResponseStatus status,
        const json &responseJson,
        const string &errorMessage = "");

    /**
     * Attempts to map observer state string to typed enum.
     */
    static bool tryParseState(
        const string &stateRaw,
        ClaimState &stateOut);

private:
    ClaimState mState;
    map<PaymentNodeID, sphincs::Signature::Shared> mVotes;
    string mRejectionSignature;
};

#endif // VTCPD_GETCLAIMSTATUSRPCRESPONSE_H
