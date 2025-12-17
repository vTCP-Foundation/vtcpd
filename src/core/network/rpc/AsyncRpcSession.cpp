#include "AsyncRpcSession.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>

#include <sstream>
#include <utility>

using namespace std::chrono;


AsyncRpcSession::AsyncRpcSession(
    IOCtx &ioCtx,
    IPv4WithPortAddress::Shared observerAddress,
    RpcRequest::Shared request,
    const string &serializedRequest,
    CompletionCallback onComplete,
    Logger &logger) :
    mIOCtx(ioCtx),
    mResolver(ioCtx),
    mSocket(ioCtx),
    mTimeoutTimer(ioCtx),
    mObserverAddress(move(observerAddress)),
    mRequest(move(request)),
    mRequestPayload(serializedRequest),
    mOnComplete(move(onComplete)),
    mCompleted(false),
    mLogger(logger)
{
    // Ensure newline termination for read_until('\n') symmetry.
    if (!mRequestPayload.empty() && mRequestPayload.back() != '\n') {
        mRequestPayload.push_back('\n');
    }
}

void AsyncRpcSession::start()
{
    // Async chain kicks off once timeout guard is armed.
    scheduleTimeout();

    auto self = shared_from_this();
    mResolver.async_resolve(
        mObserverAddress->host(),
        to_string(mObserverAddress->port()),
        [self](const boost::system::error_code &ec, const TCPResolver::results_type &results) {
            self->onResolve(ec, results);
        });
}

void AsyncRpcSession::cancel()
{
    complete(
        RpcResponseStatus::NetworkError,
        "",
        "Async RPC session cancelled");
}

void AsyncRpcSession::scheduleTimeout()
{
    mTimeoutTimer.expires_after(
        milliseconds(kObserverRpcTimeoutMs));

    auto self = shared_from_this();
    mTimeoutTimer.async_wait(
        [self](const boost::system::error_code &ec) {
            self->onTimeout(ec);
        });
}

void AsyncRpcSession::onResolve(
    const boost::system::error_code &ec,
    const TCPResolver::results_type &results)
{
    if (mCompleted.load()) {
        return;
    }

    if (ec) {
        complete(
            RpcResponseStatus::NetworkError,
            "",
            string("Failed to resolve observer address: ") + ec.message());
        return;
    }

    auto self = shared_from_this();
    boost::asio::async_connect(
        mSocket,
        results,
        [self](const boost::system::error_code &connectEc, const TCPResolver::results_type::endpoint_type &endpoint) {
            self->onConnect(connectEc, endpoint);
        });
}

void AsyncRpcSession::onConnect(
    const boost::system::error_code &ec,
    const TCPResolver::results_type::endpoint_type &endpoint)
{
    if (mCompleted.load()) {
        return;
    }

    if (ec) {
        complete(
            RpcResponseStatus::NetworkError,
            "",
            string("Failed to connect to observer endpoint ") + endpoint.address().to_string() +
                ":" + to_string(endpoint.port()) + " - " + ec.message());
        return;
    }

    auto self = shared_from_this();
    boost::asio::async_write(
        mSocket,
        boost::asio::buffer(mRequestPayload),
        [self](const boost::system::error_code &writeEc, size_t bytesTransferred) {
            self->onWrite(writeEc, bytesTransferred);
        });
}

void AsyncRpcSession::onWrite(
    const boost::system::error_code &ec,
    size_t bytesTransferred)
{
    (void)bytesTransferred;

    if (mCompleted.load()) {
        return;
    }

    if (ec) {
        complete(
            RpcResponseStatus::NetworkError,
            "",
            string("Failed to write RPC request: ") + ec.message());
        return;
    }

    auto self = shared_from_this();
    boost::asio::async_read_until(
        mSocket,
        mResponseBuffer,
        '\n',
        [self](const boost::system::error_code &readEc, size_t bytesRead) {
            self->onRead(readEc, bytesRead);
        });
}

void AsyncRpcSession::onRead(
    const boost::system::error_code &ec,
    size_t bytesTransferred)
{
    (void)bytesTransferred;

    if (mCompleted.load()) {
        return;
    }

    if (ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }

        complete(
            RpcResponseStatus::NetworkError,
            "",
            string("Failed to read RPC response: ") + ec.message());
        return;
    }

    // Extract single line response payload.
    istream responseStream(&mResponseBuffer);
    string responseLine;
    getline(responseStream, responseLine);

    complete(
        RpcResponseStatus::Success,
        responseLine,
        "");
}

void AsyncRpcSession::onTimeout(
    const boost::system::error_code &ec)
{
    if (ec == boost::asio::error::operation_aborted) {
        return;
    }

    complete(
        RpcResponseStatus::Timeout,
        "",
        "Observer RPC timed out");
}

void AsyncRpcSession::complete(
    RpcResponseStatus status,
    const string &responseBody,
    const string &errorMessage)
{
    bool expected = false;
    if (!mCompleted.compare_exchange_strong(expected, true)) {
        return;
    }

    // Prevent further invocations from timeout or socket events.
    mTimeoutTimer.cancel();

    boost::system::error_code closeEc;
    if (mSocket.is_open()) {
        mSocket.close(closeEc);
    }

    if (!mOnComplete) {
        return;
    }

    try {
        mOnComplete(
            mRequest,
            status,
            responseBody,
            errorMessage);
    } catch (const exception &e) {
        error() << "Completion handler threw exception: " << e.what();
    }
}

string AsyncRpcSession::logHeader() const
{
    return "AsyncRpcSession";
}

LoggerStream AsyncRpcSession::debug() const
{
    return mLogger.debug(logHeader());
}

LoggerStream AsyncRpcSession::info() const
{
    return mLogger.info(logHeader());
}

LoggerStream AsyncRpcSession::warning() const
{
    return mLogger.warning(logHeader());
}

LoggerStream AsyncRpcSession::error() const
{
    return mLogger.error(logHeader());
}
