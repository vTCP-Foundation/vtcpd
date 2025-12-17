#include "AcceptClaimRpcRequest.h"

#include <boost/uuid/uuid_io.hpp>

namespace {
constexpr int kJsonRpcId = 1;
constexpr char kMethodName[] = "RPCService.AcceptClaim";
}

AcceptClaimRpcRequest::AcceptClaimRpcRequest(
    const TransactionUUID &transactionUUID,
    const TransactionUUID &claimTransactionUUID,
    const BlockNumber maxClaimBlockNumber,
    const map<PaymentNodeID, sphincs::PublicKey::Shared> &participantsPublicKeys,
    sphincs::PublicKey::Shared publicKey,
    sphincs::Signature::Shared signature) :
    RpcRequest(transactionUUID),
    mClaimTransactionUUID(claimTransactionUUID),
    mMaxClaimBlockNumber(maxClaimBlockNumber),
    mParticipantsPublicKeys(participantsPublicKeys),
    mPublicKey(move(publicKey)),
    mSignature(move(signature))
{}

RpcMethod AcceptClaimRpcRequest::method() const
{
    return RpcMethod::AcceptClaim;
}

const TransactionUUID& AcceptClaimRpcRequest::claimTransactionUUID() const
{
    return mClaimTransactionUUID;
}

BlockNumber AcceptClaimRpcRequest::maxClaimBlockNumber() const
{
    return mMaxClaimBlockNumber;
}

const map<PaymentNodeID, sphincs::PublicKey::Shared>& AcceptClaimRpcRequest::participantsPublicKeys() const
{
    return mParticipantsPublicKeys;
}

sphincs::PublicKey::Shared AcceptClaimRpcRequest::publicKey() const
{
    return mPublicKey;
}

sphincs::Signature::Shared AcceptClaimRpcRequest::signature() const
{
    return mSignature;
}

json AcceptClaimRpcRequest::toJson() const
{
    json::array_t participantsArray;
    for (const auto &participant : mParticipantsPublicKeys) {
        const auto &publicKey = participant.second;
        participantsArray.push_back({
            {"index", participant.first},
            {"public_key", publicKey != nullptr ? publicKey->toString() : ""}
        });
    }

    json::array_t params = {
        json::object({
            {"transaction_uuid", boost::uuids::to_string(mClaimTransactionUUID)},
            {"max_claim_block_number", mMaxClaimBlockNumber},
            {"participants", participantsArray},
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
