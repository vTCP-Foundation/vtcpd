#include "GetBlockNumberRpcResponse.h"

#include <exception>

namespace {
/**
 * Extracts block_number from response; returns pair<success, value>.
 */
pair<bool, BlockNumber> parseBlockNumber(const json &responseJson)
{
    const auto &result = responseJson.at("result");
    return {true, result.at("block_number").get<BlockNumber>()};
}
}

GetBlockNumberRpcResponse::GetBlockNumberRpcResponse(
    const TransactionUUID &transactionUUID,
    const RpcResponseStatus status,
    const BlockNumber blockNumber,
    const string &errorMessage) :
    RpcResponse(transactionUUID, status, errorMessage),
    mBlockNumber(blockNumber)
{}

RpcMethod GetBlockNumberRpcResponse::method() const
{
    return RpcMethod::GetBlockNumber;
}

BlockNumber GetBlockNumberRpcResponse::blockNumber() const
{
    return mBlockNumber;
}

GetBlockNumberRpcResponse::Shared GetBlockNumberRpcResponse::fromJson(
    const TransactionUUID &transactionUUID,
    const RpcResponseStatus status,
    const json &responseJson,
    const string &errorMessage)
{
    if (status != RpcResponseStatus::Success) {
        // Transport-level issue, return metadata-only response.
        return make_shared<GetBlockNumberRpcResponse>(
            transactionUUID,
            status,
            0,
            errorMessage);
    }

    try {
        const auto [parsed, value] = parseBlockNumber(responseJson);
        if (!parsed) {
            return make_shared<GetBlockNumberRpcResponse>(
                transactionUUID,
                RpcResponseStatus::ParseError,
                0,
                "Invalid GetCurrentBlock response structure");
        }

        return make_shared<GetBlockNumberRpcResponse>(
            transactionUUID,
            RpcResponseStatus::Success,
            value);

    } catch (const exception &e) {
        return make_shared<GetBlockNumberRpcResponse>(
            transactionUUID,
            RpcResponseStatus::ParseError,
            0,
            string("Failed to parse GetCurrentBlock response: ") + e.what());
    }
}
