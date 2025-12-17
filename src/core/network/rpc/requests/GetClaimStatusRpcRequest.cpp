#include "GetClaimStatusRpcRequest.h"

#include <boost/uuid/uuid_io.hpp>

namespace {
constexpr int kJsonRpcId = 1;
constexpr char kMethodName[] = "RPCService.GetClaimStatus";
}

GetClaimStatusRpcRequest::GetClaimStatusRpcRequest(
    const TransactionUUID &transactionUUID,
    const TransactionUUID &claimTransactionUUID,
    const BlockNumber maxClaimBlockNumber) :
    RpcRequest(transactionUUID),
    mClaimTransactionUUID(claimTransactionUUID),
    mMaxClaimBlockNumber(maxClaimBlockNumber)
{}

RpcMethod GetClaimStatusRpcRequest::method() const
{
    return RpcMethod::GetClaimStatus;
}

const TransactionUUID& GetClaimStatusRpcRequest::claimTransactionUUID() const
{
    return mClaimTransactionUUID;
}

BlockNumber GetClaimStatusRpcRequest::maxClaimBlockNumber() const
{
    return mMaxClaimBlockNumber;
}

json GetClaimStatusRpcRequest::toJson() const
{
    json::array_t params = {
        json::object({
            {"transaction_uuid", boost::uuids::to_string(mClaimTransactionUUID)},
            {"max_claim_block_number", mMaxClaimBlockNumber}
        })
    };

    return json{
        {"method", kMethodName},
        {"params", params},
        {"id", kJsonRpcId}
    };
}
