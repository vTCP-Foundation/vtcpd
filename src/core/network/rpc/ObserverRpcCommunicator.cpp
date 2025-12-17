#include "ObserverRpcCommunicator.h"

#include "requests/AcceptClaimRpcRequest.h"
#include "requests/GetBlockNumberRpcRequest.h"
#include "requests/GetClaimStatusRpcRequest.h"
#include "requests/GetClaimStatusesRpcRequest.h"
#include "requests/SubmitClaimVotesRpcRequest.h"
#include "responses/AcceptClaimRpcResponse.h"
#include "responses/GetBlockNumberRpcResponse.h"
#include "responses/GetClaimStatusRpcResponse.h"
#include "responses/GetClaimStatusesRpcResponse.h"
#include "responses/SubmitClaimVotesRpcResponse.h"
#include "../../../logger/Logger.h"
#include "../../../../libs/json/json.h"

#include <exception>
#include <memory>
#include <stdexcept>
#include <utility>

using json = nlohmann::json;
using namespace std;

namespace {
/**
 * Minimal fallback response for unsupported or unexpected methods.
 */
class GenericRpcResponse : public RpcResponse
{
public:
    GenericRpcResponse(
        const TransactionUUID &transactionUUID,
        RpcMethod method,
        RpcResponseStatus status,
        const string &errorMessage) :
        RpcResponse(transactionUUID, status, errorMessage),
        mMethod(method)
    {}

    RpcMethod method() const override
    {
        return mMethod;
    }

private:
    RpcMethod mMethod;
};

/**
 * Maps RpcMethod enum to human-readable string for logging.
 */
string methodToString(
    const RpcMethod method)
{
    switch (method) {
    case RpcMethod::GetBlockNumber:
        return "GetBlockNumber";
    case RpcMethod::AcceptClaim:
        return "AcceptClaim";
    case RpcMethod::GetClaimStatus:
        return "GetClaimStatus";
    case RpcMethod::SubmitClaimVotes:
        return "SubmitClaimVotes";
    case RpcMethod::GetClaimStatuses:
        return "GetClaimStatuses";
    default:
        return "Unknown";
    }
}

/**
 * Builds concrete RpcResponse based on request method and status.
 */
RpcResponse::Shared buildTypedResponse(
    const RpcRequest::Shared &request,
    const RpcResponseStatus status,
    const json &responseJson,
    const string &errorMessage)
{
    const auto &transactionUUID = request->transactionUUID();
    switch (request->method()) {
    case RpcMethod::GetBlockNumber:
        return GetBlockNumberRpcResponse::fromJson(
            transactionUUID,
            status,
            responseJson,
            errorMessage);
    case RpcMethod::AcceptClaim:
        return AcceptClaimRpcResponse::fromJson(
            transactionUUID,
            status,
            responseJson,
            errorMessage);
    case RpcMethod::GetClaimStatus:
        return GetClaimStatusRpcResponse::fromJson(
            transactionUUID,
            status,
            responseJson,
            errorMessage);
    case RpcMethod::SubmitClaimVotes:
        return SubmitClaimVotesRpcResponse::fromJson(
            transactionUUID,
            status,
            responseJson,
            errorMessage);
    case RpcMethod::GetClaimStatuses:
        return GetClaimStatusesRpcResponse::fromJson(
            transactionUUID,
            status,
            responseJson,
            errorMessage);
    default:
        return make_shared<GenericRpcResponse>(
            transactionUUID,
            request->method(),
            RpcResponseStatus::RpcError,
            errorMessage.empty() ? "Unsupported RPC method" : errorMessage);
    }
}
}

ObserverRpcCommunicator::ObserverRpcCommunicator(
    IOCtx &ioCtx,
    IPv4WithPortAddress::Shared observerAddress,
    Logger &logger) :
    mIOCtx(ioCtx),
    mObserverAddress(move(observerAddress)),
    mLogger(logger)
{}

void ObserverRpcCommunicator::sendRequest(
    RpcRequest::Shared request)
{
    if (!request) {
        warning() << "sendRequest called with nullptr request";
        return;
    }

    if (mObserverAddress == nullptr) {
        error() << "Observer address is not configured";
        auto response = deserializeResponse(
            request,
            RpcResponseStatus::NetworkError,
            "",
            "Observer address is not configured");
        if (response != nullptr) {
            rpcResponseSignal(response);
        }
        return;
    }

    string serializedRequest;
    try {
        serializedRequest = serializeRequest(request);
    } catch (const exception &e) {
        error() << "Failed to serialize RPC request for method "
                << methodToString(request->method()) << ": " << e.what();
        auto response = deserializeResponse(
            request,
            RpcResponseStatus::ParseError,
            "",
            string("Failed to serialize RPC request: ") + e.what());
        if (response != nullptr) {
            rpcResponseSignal(response);
        }
        return;
    }

    info() << "Sending RPC method " << methodToString(request->method())
           << " for transaction " << request->transactionUUID()
           << " to observer " << mObserverAddress->fullAddress();

    auto session = make_shared<AsyncRpcSession>(
        mIOCtx,
        mObserverAddress,
        request,
        serializedRequest,
        [this](RpcRequest::Shared req,
               RpcResponseStatus status,
               const string &responseBody,
               const string &errorMessage) {
            auto response = deserializeResponse(
                req,
                status,
                responseBody,
                errorMessage);
            if (response != nullptr) {
                rpcResponseSignal(response);
            }
        },
        mLogger);

    session->start();
}

void ObserverRpcCommunicator::setObserverAddress(
    IPv4WithPortAddress::Shared address)
{
    mObserverAddress = move(address);
}

IPv4WithPortAddress::Shared ObserverRpcCommunicator::observerAddress() const
{
    return mObserverAddress;
}

string ObserverRpcCommunicator::serializeRequest(
    const RpcRequest::Shared &request) const
{
    switch (request->method()) {
    case RpcMethod::GetBlockNumber: {
        auto typed = dynamic_pointer_cast<GetBlockNumberRpcRequest>(request);
        if (typed == nullptr) {
            throw runtime_error("GetBlockNumber request type mismatch");
        }
        return typed->toJson().dump();
    }
    case RpcMethod::AcceptClaim: {
        auto typed = dynamic_pointer_cast<AcceptClaimRpcRequest>(request);
        if (typed == nullptr) {
            throw runtime_error("AcceptClaim request type mismatch");
        }
        return typed->toJson().dump();
    }
    case RpcMethod::GetClaimStatus: {
        auto typed = dynamic_pointer_cast<GetClaimStatusRpcRequest>(request);
        if (typed == nullptr) {
            throw runtime_error("GetClaimStatus request type mismatch");
        }
        return typed->toJson().dump();
    }
    case RpcMethod::SubmitClaimVotes: {
        auto typed = dynamic_pointer_cast<SubmitClaimVotesRpcRequest>(request);
        if (typed == nullptr) {
            throw runtime_error("SubmitClaimVotes request type mismatch");
        }
        return typed->toJson().dump();
    }
    case RpcMethod::GetClaimStatuses: {
        auto typed = dynamic_pointer_cast<GetClaimStatusesRpcRequest>(request);
        if (typed == nullptr) {
            throw runtime_error("GetClaimStatuses request type mismatch");
        }
        return typed->toJson().dump();
    }
    default:
        throw runtime_error("Unsupported RPC method for serialization");
    }
}

RpcResponse::Shared ObserverRpcCommunicator::deserializeResponse(
    const RpcRequest::Shared &request,
    RpcResponseStatus status,
    const string &responseBody,
    const string &transportError) const
{
    string errorMessage = transportError;
    json responseJson = json::object();

    // Attempt JSON parsing only when transport was successful.
    if (status == RpcResponseStatus::Success) {
        if (responseBody.empty()) {
            status = RpcResponseStatus::ParseError;
            errorMessage = "Observer RPC returned empty response";
        } else {
            try {
                responseJson = json::parse(responseBody);
            } catch (const exception &e) {
                status = RpcResponseStatus::ParseError;
                errorMessage = string("Failed to parse observer RPC JSON: ") + e.what();
            }
        }
    }

    if (status == RpcResponseStatus::Success) {
        const auto errorIt = responseJson.find("error");
        if (errorIt != responseJson.end() && !errorIt->is_null()) {
            status = RpcResponseStatus::RpcError;
            errorMessage = errorIt->is_string() ? errorIt->get<string>() : errorIt->dump();
        }
    }

    return buildTypedResponse(
        request,
        status,
        responseJson,
        errorMessage);
}

string ObserverRpcCommunicator::logHeader() const
{
    return "ObserverRpcCommunicator";
}

LoggerStream ObserverRpcCommunicator::debug() const
{
    return mLogger.debug(logHeader());
}

LoggerStream ObserverRpcCommunicator::info() const
{
    return mLogger.info(logHeader());
}

LoggerStream ObserverRpcCommunicator::warning() const
{
    return mLogger.warning(logHeader());
}

LoggerStream ObserverRpcCommunicator::error() const
{
    return mLogger.error(logHeader());
}
