#include "SubmitClaimVotesRpcRequest.h"

#include <boost/uuid/uuid_io.hpp>

namespace {
constexpr int kJsonRpcId = 1;
constexpr char kMethodName[] = "RPCService.SubmitClaimVotes";
}

SubmitClaimVotesRpcRequest::SubmitClaimVotesRpcRequest(
    const TransactionUUID &transactionUUID,
    const TransactionUUID &claimTransactionUUID,
    const BlockNumber maxClaimBlockNumber,
    const map<PaymentNodeID, sphincs::Signature::Shared> &votes,
    sphincs::PublicKey::Shared publicKey,
    sphincs::Signature::Shared signature) :
    RpcRequest(transactionUUID),
    mClaimTransactionUUID(claimTransactionUUID),
    mMaxClaimBlockNumber(maxClaimBlockNumber),
    mVotes(votes),
    mPublicKey(move(publicKey)),
    mSignature(move(signature))
{}

RpcMethod SubmitClaimVotesRpcRequest::method() const
{
    return RpcMethod::SubmitClaimVotes;
}

const TransactionUUID& SubmitClaimVotesRpcRequest::claimTransactionUUID() const
{
    return mClaimTransactionUUID;
}

BlockNumber SubmitClaimVotesRpcRequest::maxClaimBlockNumber() const
{
    return mMaxClaimBlockNumber;
}

const map<PaymentNodeID, sphincs::Signature::Shared>& SubmitClaimVotesRpcRequest::votes() const
{
    return mVotes;
}

sphincs::PublicKey::Shared SubmitClaimVotesRpcRequest::publicKey() const
{
    return mPublicKey;
}

sphincs::Signature::Shared SubmitClaimVotesRpcRequest::signature() const
{
    return mSignature;
}

json SubmitClaimVotesRpcRequest::toJson() const
{
    json::array_t votesArray;
    for (const auto &vote : mVotes) {
        const auto &signature = vote.second;
        votesArray.push_back({
            {"index", vote.first},
            {"signature", signature != nullptr ? signature->toString() : ""}
        });
    }

    json::array_t params = {
        json::object({
            {"transaction_uuid", boost::uuids::to_string(mClaimTransactionUUID)},
            {"max_claim_block_number", mMaxClaimBlockNumber},
            {"votes", votesArray},
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
