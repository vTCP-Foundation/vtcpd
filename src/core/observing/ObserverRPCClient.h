#ifndef VTCPD_OBSERVERRPCCLIENT_H
#define VTCPD_OBSERVERRPCCLIENT_H

#include "../contractors/addresses/IPv4WithPortAddress.h"
#include "../common/Types.h"
#include "../logger/Logger.h"
#include "ObservingPaymentClaim.h"
#include "../transactions/transactions/base/TransactionUUID.h"
#include "../../libs/json/json.h"

#include <boost/asio.hpp>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using boost::asio::ip::tcp;
using json = nlohmann::json;

class ObserverRPCClient
{

public:
    struct ClaimStatusInfo {
        TransactionUUID transactionUUID;
        BlockNumber maxClaimBlockNumber;
        std::string state;
    };

public:
    ObserverRPCClient(
        IOCtx &ioCtx,
        Logger &logger);

    BlockNumber getBlockNumber(
        IPv4WithPortAddress::Shared observerAddress);

    bool acceptClaim(
        IPv4WithPortAddress::Shared observerAddress,
        const ObservingPaymentClaim &claim);

    ObservingPaymentClaim::ClaimStatus getClaimStatus(
        IPv4WithPortAddress::Shared observerAddress,
        const TransactionUUID &transactionUUID,
        BlockNumber maxClaimBlockNumber);

    std::map<PaymentNodeID, sphincs::Signature::Shared> getClaimVotes(
        IPv4WithPortAddress::Shared observerAddress,
        const TransactionUUID &transactionUUID,
        BlockNumber maxClaimBlockNumber);

    std::optional<std::string> getRejectionSignature(
        IPv4WithPortAddress::Shared observerAddress,
        const TransactionUUID &transactionUUID,
        BlockNumber maxClaimBlockNumber);

    bool submitClaimVotes(
        IPv4WithPortAddress::Shared observerAddress,
        const TransactionUUID &transactionUUID,
        BlockNumber maxClaimBlockNumber,
        const std::map<PaymentNodeID, sphincs::Signature::Shared> &participantsSignatures,
        std::string &responseMessage);

    std::vector<ClaimStatusInfo> getClaimStatuses(
        IPv4WithPortAddress::Shared observerAddress,
        const std::vector<std::pair<TransactionUUID, BlockNumber>> &transactions);

protected:
    static string logHeader();

    LoggerStream error() const;

    LoggerStream debug() const;

    LoggerStream info() const;

    LoggerStream warning() const;

private:
    json performRpcCall(
        IPv4WithPortAddress::Shared observerAddress,
        const string &method,
        const json::array_t &params);

private:
    IOCtx &mIOCtx;
    Logger &mLogger;
};

#endif // VTCPD_OBSERVERRPCCLIENT_H
