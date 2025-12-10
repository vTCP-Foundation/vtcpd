#include "ObserverRPCClient.h"

#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <sstream>
#include "../../libs/json/json.h"

using json = nlohmann::json;

ObserverRPCClient::ObserverRPCClient(
    IOCtx &ioCtx,
    Logger &logger):
    mIOCtx(ioCtx),
    mLogger(logger)
{}

BlockNumber ObserverRPCClient::getBlockNumber(
    IPv4WithPortAddress::Shared observerAddress)
{
#ifdef DEBUG_LOG_OBSEVING_HANDLER
    debug() << "getBlockNumber from " << observerAddress->fullAddress();
#endif

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

        // Prepare JSON-RPC 1.0 request
        json request = {
            {"method", "RPCService.GetCurrentBlock"},
            {"params", json::array({json::object()})},
            {"id", 1}
        };

        string requestStr = request.dump() + "\n";

#ifdef DEBUG_LOG_OBSEVING_HANDLER
        debug() << "Sending RPC request: " << request.dump();
#endif

        // Send request
        boost::asio::write(
            socket,
            boost::asio::buffer(requestStr));

        // Read response until newline
        boost::asio::streambuf responseBuffer;
        boost::asio::read_until(socket, responseBuffer, '\n', errorCode);

        if (errorCode && errorCode != boost::asio::error::eof) {
            throw errorCode;
        }

        // Parse response
        std::istream responseStream(&responseBuffer);
        string responseLine;
        std::getline(responseStream, responseLine);

#ifdef DEBUG_LOG_OBSEVING_HANDLER
        debug() << "Received RPC response: " << responseLine;
#endif

        json response = json::parse(responseLine);

        // Check for errors
        if (response.contains("error") && !response["error"].is_null()) {
            string errorMsg = response["error"].is_string()
                ? response["error"].get<string>()
                : response["error"].dump();
            warning() << "RPC error: " << errorMsg;
            throw runtime_error("RPC error: " + errorMsg);
        }

        // Extract block number
        if (response.contains("result") && response["result"].contains("block_number")) {
            BlockNumber blockNumber = response["result"]["block_number"].get<BlockNumber>();

#ifdef DEBUG_LOG_OBSEVING_HANDLER
            debug() << "Received block number: " << blockNumber;
#endif

            socket.close();
            return blockNumber;
        } else {
            warning() << "Invalid RPC response format: missing block_number";
            throw runtime_error("Invalid RPC response format");
        }

    } catch (const boost::system::error_code &e) {
        error() << "Network error: " << e.message();
        throw;
    } catch (const json::exception &e) {
        error() << "JSON parsing error: " << e.what();
        throw;
    } catch (const std::exception &e) {
        error() << "Error getting block number: " << e.what();
        throw;
    }
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
