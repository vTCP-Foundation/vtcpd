#ifndef VTCPD_OBSERVERRPCCOMMUNICATOR_H
#define VTCPD_OBSERVERRPCCOMMUNICATOR_H

#include "AsyncRpcSession.h"
#include "RpcRequest.h"
#include "RpcResponse.h"
#include "../../contractors/addresses/IPv4WithPortAddress.h"
#include "../communicator/internal/common/Types.h"
#include "../../logger/Logger.h"

#include <string>


/**
 * Entry point for async RPC exchanges with the observer.
 * Handles JSON (de)serialization, AsyncRpcSession lifecycle, and exposes responses via signal.
 */
class ObserverRpcCommunicator
{
public:
    using RpcResponseSignal = signals::signal<void(RpcResponse::Shared)>;

public:
    /**
     * Configures communicator with IO context, observer address, and logger.
     */
    ObserverRpcCommunicator(
        IOCtx &ioCtx,
        IPv4WithPortAddress::Shared observerAddress,
        Logger &logger);

    /**
     * Serializes and sends the provided RPC request via AsyncRpcSession.
     */
    void sendRequest(
        RpcRequest::Shared request);

    /**
     * Updates observer address used for subsequent requests.
     */
    void setObserverAddress(
        IPv4WithPortAddress::Shared address);

    /**
     * Returns current observer address configuration.
     */
    IPv4WithPortAddress::Shared observerAddress() const;

public:
    mutable RpcResponseSignal rpcResponseSignal;

private:
    /**
     * Converts typed RpcRequest into JSON-RPC string payload.
     */
    string serializeRequest(
        const RpcRequest::Shared &request) const;

    /**
     * Translates transport completion into typed RpcResponse and emits signal.
     */
    RpcResponse::Shared deserializeResponse(
        const RpcRequest::Shared &request,
        RpcResponseStatus status,
        const string &responseBody,
        const string &transportError) const;

    /**
     * Returns subsystem name for logs.
     */
    string logHeader() const;

    LoggerStream debug() const;
    LoggerStream info() const;
    LoggerStream warning() const;
    LoggerStream error() const;

private:
    IOCtx &mIOCtx;
    IPv4WithPortAddress::Shared mObserverAddress;
    Logger &mLogger;
};


#endif // VTCPD_OBSERVERRPCCOMMUNICATOR_H
