#ifndef VTCPD_ASYNCRPCSESSION_H
#define VTCPD_ASYNCRPCSESSION_H

#include "../../contractors/addresses/IPv4WithPortAddress.h"
#include "../communicator/internal/common/Types.h"
#include "../../logger/Logger.h"
#include "RpcRequest.h"
#include "RpcResponseStatus.h"

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/streambuf.hpp>

#include <atomic>
#include <functional>
#include <memory>
#include <string>

using namespace std;


/**
 * Handles a single async TCP RPC exchange to the observer (resolve -> connect -> write -> read).
 * Transport-only: serialization and deserialization are handled by ObserverRpcCommunicator.
 */
class AsyncRpcSession : public enable_shared_from_this<AsyncRpcSession>
{
public:
    using Shared = shared_ptr<AsyncRpcSession>;
    using CompletionCallback = function<void(
        RpcRequest::Shared request,
        RpcResponseStatus status,
        const string &responseBody,
        const string &errorMessage)>;

    // TODO: consider limiting concurrent sessions once higher-level pooling/backpressure is introduced.
    static constexpr uint32_t kObserverRpcTimeoutMs = 10'000;

public:
    /**
     * Prepares transport session with serialized payload and completion callback.
     */
    AsyncRpcSession(
        IOCtx &ioCtx,
        IPv4WithPortAddress::Shared observerAddress,
        RpcRequest::Shared request,
        const string &serializedRequest,
        CompletionCallback onComplete,
        Logger &logger);

    /**
     * Starts async chain: resolve -> connect -> write -> read_until('\n').
     */
    void start();

    /**
     * Cancels in-flight operations and reports cancellation as NetworkError.
     */
    void cancel();

private:
    /**
     * Arms the timeout timer for the configured deadline.
     */
    void scheduleTimeout();

    /**
     * Invoked when timeout expires.
     */
    void onTimeout(
        const boost::system::error_code &ec);

    /**
     * Invoked when resolve finishes.
     */
    void onResolve(
        const boost::system::error_code &ec,
        const TCPResolver::results_type &results);

    /**
     * Invoked when connect finishes.
     */
    void onConnect(
        const boost::system::error_code &ec,
        const TCPResolver::results_type::endpoint_type &endpoint);

    /**
     * Invoked when write finishes.
     */
    void onWrite(
        const boost::system::error_code &ec,
        size_t bytesTransferred);

    /**
     * Invoked when read_until finishes.
     */
    void onRead(
        const boost::system::error_code &ec,
        size_t bytesTransferred);

    /**
     * Triggers single-shot completion with mapped status and optional response body.
     */
    void complete(
        RpcResponseStatus status,
        const string &responseBody,
        const string &errorMessage);

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
    TCPResolver mResolver;
    TCPSocket mSocket;
    boost::asio::steady_timer mTimeoutTimer;

    IPv4WithPortAddress::Shared mObserverAddress;
    RpcRequest::Shared mRequest;
    string mRequestPayload;
    CompletionCallback mOnComplete;

    boost::asio::streambuf mResponseBuffer;
    atomic<bool> mCompleted;
    Logger &mLogger;
};

#endif // VTCPD_ASYNCRPCSESSION_H
