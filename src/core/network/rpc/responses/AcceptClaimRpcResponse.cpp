#include "AcceptClaimRpcResponse.h"

#include <exception>

AcceptClaimRpcResponse::AcceptClaimRpcResponse(
    const TransactionUUID &transactionUUID,
    const RpcResponseStatus status,
    const bool success,
    const string &message,
    const string &errorMessage) :
    RpcResponse(transactionUUID, status, errorMessage),
    mSuccess(success),
    mMessage(message)
{}

RpcMethod AcceptClaimRpcResponse::method() const
{
    return RpcMethod::AcceptClaim;
}

bool AcceptClaimRpcResponse::success() const
{
    return mSuccess;
}

const string& AcceptClaimRpcResponse::message() const
{
    return mMessage;
}

AcceptClaimRpcResponse::Shared AcceptClaimRpcResponse::fromJson(
    const TransactionUUID &transactionUUID,
    const RpcResponseStatus status,
    const json &responseJson,
    const string &errorMessage)
{
    if (status != RpcResponseStatus::Success) {
        return make_shared<AcceptClaimRpcResponse>(
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

        return make_shared<AcceptClaimRpcResponse>(
            transactionUUID,
            RpcResponseStatus::Success,
            success,
            message);

    } catch (const exception &e) {
        return make_shared<AcceptClaimRpcResponse>(
            transactionUUID,
            RpcResponseStatus::ParseError,
            false,
            "",
            string("Failed to parse AcceptClaim response: ") + e.what());
    }
}
