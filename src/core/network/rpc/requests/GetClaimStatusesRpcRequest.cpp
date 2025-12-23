#include "GetClaimStatusesRpcRequest.h"

#include <boost/uuid/uuid_io.hpp>

namespace {
constexpr int kJsonRpcId = 1;
constexpr char kMethodName[] = "RPCService.GetClaimStatuses";
}

GetClaimStatusesRpcRequest::GetClaimStatusesRpcRequest(
    const TransactionUUID &transactionUUID,
    const vector<ClaimInfo> &claims) :
    RpcRequest(transactionUUID),
    mClaims(claims)
{}

RpcMethod GetClaimStatusesRpcRequest::method() const
{
    return RpcMethod::GetClaimStatuses;
}

const vector<GetClaimStatusesRpcRequest::ClaimInfo>& GetClaimStatusesRpcRequest::claims() const
{
    return mClaims;
}

sphincs::PublicKey::Shared GetClaimStatusesRpcRequest::publicKey() const
{
    return mPublicKey;
}

sphincs::Signature::Shared GetClaimStatusesRpcRequest::signature() const
{
    return mSignature;
}

void GetClaimStatusesRpcRequest::setPublicKey(sphincs::PublicKey::Shared publicKey)
{
    mPublicKey = move(publicKey);
}

void GetClaimStatusesRpcRequest::setSignature(sphincs::Signature::Shared signature)
{
    mSignature = move(signature);
}

json GetClaimStatusesRpcRequest::toJson() const
{
    json::array_t claimsArray;
    for (const auto &claim : mClaims) {
        claimsArray.push_back({
            {"transaction_uuid", boost::uuids::to_string(claim.transactionUUID)},
            {"max_claim_block_number", claim.maxClaimBlockNumber}
        });
    }

    json::array_t params = {
        json::object({
            {"claims", claimsArray},
            {"public_key", mPublicKey != nullptr ? mPublicKey->toString() : ""},
            {"signature", mSignature != nullptr ? mSignature->toString() : ""}
        })
    };

    return json{
        {"method", kMethodName},
        {"params", params},
        {"id", kJsonRpcId}
    };
}
