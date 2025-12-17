#include "GetClaimStatusesRpcResponse.h"

#include <exception>
#include <stdexcept>

namespace {
/**
 * Converts observer response entry into ClaimStatus structure.
 */
GetClaimStatusesRpcResponse::ClaimStatus parseStatusEntry(const json &entry)
{
    const auto transactionUUID = TransactionUUID(entry.at("transaction_uuid").get<string>());
    const auto maxClaimBlockNumber = entry.at("max_claim_block_number").get<BlockNumber>();

    const string stateRaw = entry.at("state").get<string>();
    GetClaimStatusesRpcResponse::ClaimState mappedState;
    if (!GetClaimStatusRpcResponse::tryParseState(stateRaw, mappedState)) {
        throw runtime_error("Unknown claim state in GetClaimStatuses entry");
    }

    return {transactionUUID, maxClaimBlockNumber, mappedState};
}
}

GetClaimStatusesRpcResponse::GetClaimStatusesRpcResponse(
    const TransactionUUID &transactionUUID,
    const RpcResponseStatus status,
    const vector<ClaimStatus> &statuses,
    const string &errorMessage) :
    RpcResponse(transactionUUID, status, errorMessage),
    mStatuses(statuses)
{}

RpcMethod GetClaimStatusesRpcResponse::method() const
{
    return RpcMethod::GetClaimStatuses;
}

const vector<GetClaimStatusesRpcResponse::ClaimStatus>& GetClaimStatusesRpcResponse::statuses() const
{
    return mStatuses;
}

GetClaimStatusesRpcResponse::Shared GetClaimStatusesRpcResponse::fromJson(
    const TransactionUUID &transactionUUID,
    const RpcResponseStatus status,
    const json &responseJson,
    const string &errorMessage)
{
    if (status != RpcResponseStatus::Success) {
        return make_shared<GetClaimStatusesRpcResponse>(
            transactionUUID,
            status,
            vector<ClaimStatus>(),
            errorMessage);
    }

    try {
        vector<ClaimStatus> statuses;
        const auto &result = responseJson.at("result");
        const auto &statusesJson = result.at("statuses");
        if (!statusesJson.is_array()) {
            throw runtime_error("statuses field is not array");
        }

        statuses.reserve(statusesJson.size());
        for (const auto &entry : statusesJson) {
            statuses.push_back(parseStatusEntry(entry));
        }

        return make_shared<GetClaimStatusesRpcResponse>(
            transactionUUID,
            RpcResponseStatus::Success,
            statuses);

    } catch (const exception &e) {
        return make_shared<GetClaimStatusesRpcResponse>(
            transactionUUID,
            RpcResponseStatus::ParseError,
            vector<ClaimStatus>(),
            string("Failed to parse GetClaimStatuses response: ") + e.what());
    }
}
