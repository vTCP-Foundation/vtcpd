#include "GetClaimStatusRpcResponse.h"

#include <algorithm>
#include <exception>

namespace {
/**
 * Converts votes array into map keyed by PaymentNodeID.
 */
map<PaymentNodeID, sphincs::Signature::Shared> parseVotes(const json &votesJson)
{
    map<PaymentNodeID, sphincs::Signature::Shared> votes;
    if (!votesJson.is_array()) {
        return votes;
    }

    for (const auto &vote : votesJson) {
        const auto nodeId = vote.at("index").get<PaymentNodeID>();
        const string signatureBase64 = vote.at("signature").get<string>();
        auto signature = make_shared<sphincs::Signature>(signatureBase64);
        if (signature != nullptr && signature->isValid()) {
            votes[nodeId] = signature;
        }
    }
    return votes;
}
}

GetClaimStatusRpcResponse::GetClaimStatusRpcResponse(
    const TransactionUUID &transactionUUID,
    const RpcResponseStatus status,
    const ClaimState state,
    const map<PaymentNodeID, sphincs::Signature::Shared> &votes,
    const string &rejectionSignature,
    const string &errorMessage) :
    RpcResponse(transactionUUID, status, errorMessage),
    mState(state),
    mVotes(votes),
    mRejectionSignature(rejectionSignature)
{}

RpcMethod GetClaimStatusRpcResponse::method() const
{
    return RpcMethod::GetClaimStatus;
}

GetClaimStatusRpcResponse::ClaimState GetClaimStatusRpcResponse::state() const
{
    return mState;
}

const map<PaymentNodeID, sphincs::Signature::Shared>& GetClaimStatusRpcResponse::votes() const
{
    return mVotes;
}

const string& GetClaimStatusRpcResponse::rejectionSignature() const
{
    return mRejectionSignature;
}

bool GetClaimStatusRpcResponse::tryParseState(
    const string &stateRaw,
    ClaimState &stateOut)
{
    string state = stateRaw;
    transform(state.begin(), state.end(), state.begin(), ::tolower);

    if (state == "not found") {
        stateOut = ClaimState::NotFound;
        return true;
    }
    if (state == "observing") {
        stateOut = ClaimState::Observing;
        return true;
    }
    if (state == "approved") {
        stateOut = ClaimState::Approved;
        return true;
    }
    if (state == "rejected") {
        stateOut = ClaimState::Rejected;
        return true;
    }

    return false;
}

GetClaimStatusRpcResponse::Shared GetClaimStatusRpcResponse::fromJson(
    const TransactionUUID &transactionUUID,
    const RpcResponseStatus status,
    const json &responseJson,
    const string &errorMessage)
{
    if (status != RpcResponseStatus::Success) {
        return make_shared<GetClaimStatusRpcResponse>(
            transactionUUID,
            status,
            ClaimState::NotFound,
            map<PaymentNodeID, sphincs::Signature::Shared>(),
            "",
            errorMessage);
    }

    try {
        const auto &result = responseJson.at("result");
        const string stateRaw = result.at("state").get<string>();
        ClaimState mappedState = ClaimState::NotFound;
        if (!tryParseState(stateRaw, mappedState)) {
            return make_shared<GetClaimStatusRpcResponse>(
                transactionUUID,
                RpcResponseStatus::ParseError,
                ClaimState::NotFound,
                map<PaymentNodeID, sphincs::Signature::Shared>(),
                "",
                "Unknown claim state in GetClaimStatus response");
        }

        map<PaymentNodeID, sphincs::Signature::Shared> votes;
        string rejectionSignature;

        if (mappedState == ClaimState::Approved) {
            votes = parseVotes(result.at("votes"));
        } else if (mappedState == ClaimState::Rejected) {
            rejectionSignature = result.value("signature", "");
        }

        return make_shared<GetClaimStatusRpcResponse>(
            transactionUUID,
            RpcResponseStatus::Success,
            mappedState,
            votes,
            rejectionSignature);

    } catch (const exception &e) {
        return make_shared<GetClaimStatusRpcResponse>(
            transactionUUID,
            RpcResponseStatus::ParseError,
            ClaimState::NotFound,
            map<PaymentNodeID, sphincs::Signature::Shared>(),
            "",
            string("Failed to parse GetClaimStatus response: ") + e.what());
    }
}
