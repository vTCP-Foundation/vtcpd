#ifndef VTCPD_OBSERVERRPCCLIENT_H
#define VTCPD_OBSERVERRPCCLIENT_H

#include "../contractors/addresses/IPv4WithPortAddress.h"
#include "../common/Types.h"
#include "../logger/Logger.h"

#include <boost/asio.hpp>
#include <string>
#include <memory>

using boost::asio::ip::tcp;

class ObserverRPCClient
{

public:
    ObserverRPCClient(
        IOCtx &ioCtx,
        Logger &logger);

    BlockNumber getBlockNumber(
        IPv4WithPortAddress::Shared observerAddress);

protected:
    static string logHeader();

    LoggerStream error() const;

    LoggerStream debug() const;

    LoggerStream info() const;

    LoggerStream warning() const;

private:
    IOCtx &mIOCtx;
    Logger &mLogger;
};

#endif // VTCPD_OBSERVERRPCCLIENT_H
