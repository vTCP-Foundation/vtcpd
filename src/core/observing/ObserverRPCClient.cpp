#include "ObserverRPCClient.h"

#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <sstream>

ObserverRPCClient::ObserverRPCClient(
    IOCtx &ioCtx,
    Logger &logger):
    mIOCtx(ioCtx),
    mLogger(logger)
{}

json ObserverRPCClient::performRpcCall(
    IPv4WithPortAddress::Shared observerAddress,
    const string &method,
    const json::array_t &params)
{
    try {
        tcp::resolver resolver(mIOCtx);
        boost::system::error_code errorCode;

        auto endpoints = resolver.resolve(
            observerAddress->host(),
            to_string(observerAddress->port()),
            errorCode);

        if (errorCode) {
            throw errorCode;
        }

        tcp::socket socket(mIOCtx);
        boost::asio::connect(socket, endpoints, errorCode);

        if (errorCode) {
            throw errorCode;
        }

        json request = {
            {"method", method},
            {"params", params},
            {"id", 1}
        };

        string requestStr = request.dump() + "\n";

#ifdef DEBUG_LOG_OBSEVING_HANDLER
        debug() << "Sending RPC request to " << observerAddress->fullAddress()
                << " " << request.dump();
#endif

        boost::asio::write(
            socket,
            boost::asio::buffer(requestStr));

        boost::asio::streambuf responseBuffer;
        boost::asio::read_until(socket, responseBuffer, '\n', errorCode);

        if (errorCode && errorCode != boost::asio::error::eof) {
            throw errorCode;
        }

        std::istream responseStream(&responseBuffer);
        string responseLine;
        std::getline(responseStream, responseLine);

#ifdef DEBUG_LOG_OBSEVING_HANDLER
        debug() << "Received RPC response from " << observerAddress->fullAddress()
                << ": " << responseLine;
#endif

        json response = json::parse(responseLine);

        if (response.contains("error") && !response["error"].is_null()) {
            string errorMsg = response["error"].is_string()
                ? response["error"].get<string>()
                : response["error"].dump();
            throw runtime_error("Observer RPC error: " + errorMsg);
        }

        socket.close();
        return response;

    } catch (const boost::system::error_code &e) {
        error() << "Network error calling observer "
                << observerAddress->fullAddress() << ": " << e.message();
        throw;
    } catch (const json::exception &e) {
        error() << "JSON parsing error from observer "
                << observerAddress->fullAddress() << ": " << e.what();
        throw;
    } catch (const std::exception &e) {
        error() << "RPC call error to observer "
                << observerAddress->fullAddress() << ": " << e.what();
        throw;
    }
}

BlockNumber ObserverRPCClient::getBlockNumber(
    IPv4WithPortAddress::Shared observerAddress)
{
    auto response = performRpcCall(
        observerAddress,
        "RPCService.GetCurrentBlock",
        json::array({json::object()}));

    if (!response.contains("result") ||
            !response["result"].contains("block_number")) {
        warning() << "Invalid RPC response format: missing block_number";
        throw runtime_error("Invalid RPC response format");
    }

    return response["result"]["block_number"].get<BlockNumber>();
}

bool ObserverRPCClient::acceptClaim(
    IPv4WithPortAddress::Shared observerAddress,
    const ObservingPaymentClaim &claim)
{
    json::array_t participantsArray;
    for (const auto &participant : claim.participantsPublicKeys()) {
        participantsArray.push_back({
            {"index", participant.first},
            {"public_key", participant.second->toString()}
        });
    }

    json::array_t params = {
        json::object({
            {"transaction_uuid", boost::uuids::to_string(claim.transactionUUID())},
            {"max_claim_block_number", claim.maxBlockNumberForClaiming()},
            {"participants", participantsArray},
            {"public_key", claim.publicKey()->toString()},
            {"signature", claim.signature()->toString()}
        })
    };

    auto response = performRpcCall(
        observerAddress,
        "RPCService.AcceptClaim",
        params);

    if (!response.contains("result")) {
        throw runtime_error("Invalid RPC response: missing result");
    }

    auto result = response["result"];
    bool success = result.value("success", false);

    if (!success) {
        string message = result.contains("message") && result["message"].is_string()
            ? result["message"].get<string>()
            : "Observer claim rejected";
        throw runtime_error(message);
    }

    return true;
}

ObservingPaymentClaim::ClaimStatus ObserverRPCClient::getClaimStatus(
    IPv4WithPortAddress::Shared observerAddress,
    const TransactionUUID &transactionUUID,
    BlockNumber maxClaimBlockNumber)
{
    json::array_t params = {
        json::object({
            {"transaction_uuid", boost::uuids::to_string(transactionUUID)},
            {"max_claim_block_number", maxClaimBlockNumber}
        })
    };

    auto response = performRpcCall(
        observerAddress,
        "RPCService.GetClaimStatus",
        params);

    if (!response.contains("result") || !response["result"].contains("state")) {
        throw runtime_error("Invalid RPC response: missing state");
    }

    auto state = response["result"]["state"].get<string>();

    if (state == "not found") {
        return ObservingPaymentClaim::NoInfo;
    }
    if (state == "observing") {
        return ObservingPaymentClaim::Observing;
    }
    if (state == "approved") {
        return ObservingPaymentClaim::ParticipantsVotesPresent;
    }
    if (state == "rejected") {
        return ObservingPaymentClaim::RejectedByObserving;
    }

    throw runtime_error("Unknown claim state from observer: " + state);
}

std::map<PaymentNodeID, sphincs::Signature::Shared> ObserverRPCClient::getClaimVotes(
    IPv4WithPortAddress::Shared observerAddress,
    const TransactionUUID &transactionUUID,
    BlockNumber maxClaimBlockNumber)
{
    json::array_t params = {
        json::object({
            {"transaction_uuid", boost::uuids::to_string(transactionUUID)},
            {"max_claim_block_number", maxClaimBlockNumber}
        })
    };

    auto response = performRpcCall(
        observerAddress,
        "RPCService.GetClaimVotes",
        params);

    if (!response.contains("result") || !response["result"].contains("votes")) {
        throw runtime_error("Invalid RPC response: missing votes");
    }

    auto votesArray = response["result"]["votes"];

    if (!votesArray.is_array()) {
        throw runtime_error("Invalid RPC response: votes is not array");
    }

    map<PaymentNodeID, sphincs::Signature::Shared> signaturesMap;

    for (const auto &vote : votesArray) {
        PaymentNodeID nodeId = vote.at("index").get<PaymentNodeID>();
        string signatureBase64 = vote.at("signature").get<string>();

        auto signature = make_shared<sphincs::Signature>(signatureBase64);
        if (!signature->isValid()) {
            warning() << "Invalid signature from observer for node " << nodeId;
            continue;
        }

        signaturesMap[nodeId] = signature;
    }

    return signaturesMap;
}

std::optional<std::string> ObserverRPCClient::getRejectionSignature(
    IPv4WithPortAddress::Shared observerAddress,
    const TransactionUUID &transactionUUID,
    BlockNumber maxClaimBlockNumber)
{
    json::array_t params = {
        json::object({
            {"transaction_uuid", boost::uuids::to_string(transactionUUID)},
            {"max_claim_block_number", maxClaimBlockNumber}
        })
    };

    auto response = performRpcCall(
        observerAddress,
        "RPCService.GetRejectionSignature",
        params);

    if (!response.contains("result")) {
        throw runtime_error("Invalid RPC response: missing result");
    }

    auto result = response["result"];
    string signature = result.value("signature", "");

    if (signature.empty()) {
        return std::nullopt;
    }

    return signature;
}

bool ObserverRPCClient::submitClaimVotes(
    IPv4WithPortAddress::Shared observerAddress,
    const TransactionUUID &transactionUUID,
    BlockNumber maxClaimBlockNumber,
    const std::map<PaymentNodeID, sphincs::Signature::Shared> &participantsSignatures,
    std::string &responseMessage)
{
    json::array_t votesArray;
    for (const auto &participant : participantsSignatures) {
        votesArray.push_back({
            {"index", participant.first},
            {"signature", participant.second->toString()}
        });
    }

    json::array_t params = {
        json::object({
            {"transaction_uuid", boost::uuids::to_string(transactionUUID)},
            {"max_claim_block_number", maxClaimBlockNumber},
            {"votes", votesArray},
            {"public_key", ""},
            {"signature", ""}
        })
    };

    auto response = performRpcCall(
        observerAddress,
        "RPCService.SubmitClaimVotes",
        params);

    if (!response.contains("result")) {
        throw runtime_error("Invalid RPC response: missing result");
    }

    auto result = response["result"];
    responseMessage = result.value("message", "unknown error");
    return result.value("success", false);
}

std::vector<ObserverRPCClient::ClaimStatusInfo> ObserverRPCClient::getClaimStatuses(
    IPv4WithPortAddress::Shared observerAddress,
    const std::vector<std::pair<TransactionUUID, BlockNumber>> &transactions)
{
    json::array_t claimsArray;
    for (const auto &transaction : transactions) {
        claimsArray.push_back({
            {"transaction_uuid", boost::uuids::to_string(transaction.first)},
            {"max_claim_block_number", transaction.second}
        });
    }

    json::array_t params = {
        json::object({
            {"claims", claimsArray}
        })
    };

    auto response = performRpcCall(
        observerAddress,
        "RPCService.GetClaimStatuses",
        params);

    if (!response.contains("result") || !response["result"].contains("statuses")) {
        throw runtime_error("Invalid RPC response for claim statuses");
    }

    const auto &statuses = response["result"]["statuses"];

    if (!statuses.is_array()) {
        throw runtime_error("Invalid RPC response: statuses is not array");
    }

    vector<ClaimStatusInfo> result;
    for (const auto &claimStatus : statuses) {
        try {
            const auto uuidStr = claimStatus.at("transaction_uuid").get<string>();
            BlockNumber claimBlockNumber = claimStatus.at("max_claim_block_number").get<BlockNumber>();
            const auto state = claimStatus.at("state").get<string>();
            result.push_back({
                TransactionUUID(uuidStr),
                claimBlockNumber,
                state
            });
        } catch (const std::exception &e) {
            warning() << "Error processing claim status entry: " << e.what();
            continue;
        }
    }

    return result;
}

string ObserverRPCClient::logHeader()
{
    return "ObserverRPCClient";
}

LoggerStream ObserverRPCClient::error() const
{
    return mLogger.error(logHeader());
}

LoggerStream ObserverRPCClient::debug() const
{
    return mLogger.debug(logHeader());
}

LoggerStream ObserverRPCClient::info() const
{
    return mLogger.info(logHeader());
}

LoggerStream ObserverRPCClient::warning() const
{
    return mLogger.warning(logHeader());
}
