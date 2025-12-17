#include "SubmitClaimVotesRpcResponse.h"

#include <exception>

SubmitClaimVotesRpcResponse::SubmitClaimVotesRpcResponse(
    const TransactionUUID &transactionUUID,
    const RpcResponseStatus status,
    const bool success,
    const string &message,
    const string &errorMessage) :
    RpcResponse(transactionUUID, status, errorMessage),
    mSuccess(success),
    mMessage(message)
{}

RpcMethod SubmitClaimVotesRpcResponse::method() const
{
    return RpcMethod::SubmitClaimVotes;
}

bool SubmitClaimVotesRpcResponse::success() const
{
    return mSuccess;
}

const string& SubmitClaimVotesRpcResponse::message() const
{
    return mMessage;
}

SubmitClaimVotesRpcResponse::Shared SubmitClaimVotesRpcResponse::fromJson(
    const TransactionUUID &transactionUUID,
    const RpcResponseStatus status,
    const json &responseJson,
    const string &errorMessage)
{
    if (status != RpcResponseStatus::Success) {
        return make_shared<SubmitClaimVotesRpcResponse>(
            transactionUUID,
            status,
            false,
            "",
            errorMessage);
    }

    try {
        const auto &result = responseJson.at("result");
        const bool success = result.at("success").get<bool>();
        const string message = result.value("message", "");

        return make_shared<SubmitClaimVotesRpcResponse>(
            transactionUUID,
            RpcResponseStatus::Success,
            success,
            message);

    } catch (const exception &e) {
        return make_shared<SubmitClaimVotesRpcResponse>(
            transactionUUID,
            RpcResponseStatus::ParseError,
            false,
            "",
            string("Failed to parse SubmitClaimVotes response: ") + e.what());
    }
}
