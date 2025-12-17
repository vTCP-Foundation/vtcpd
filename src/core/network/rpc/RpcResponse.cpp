#include "RpcResponse.h"


/**
 * Stores common response metadata used by upstream routing and logging.
 */
RpcResponse::RpcResponse(
    const TransactionUUID &transactionUUID,
    const RpcResponseStatus status,
    const string &errorMessage) :
    mTransactionUUID(transactionUUID),
    mStatus(status),
    mErrorMessage(errorMessage)
{
}

/**
 * Returns the transaction UUID that initiated the request.
 */
const TransactionUUID& RpcResponse::transactionUUID() const
{
    return mTransactionUUID;
}

/**
 * Provides the transport or RPC status code.
 */
RpcResponseStatus RpcResponse::status() const
{
    return mStatus;
}

/**
 * Returns a descriptive error message when available.
 */
const string& RpcResponse::errorMessage() const
{
    return mErrorMessage;
}

/**
 * Convenience helper that checks if the status marks success.
 */
bool RpcResponse::isSuccess() const
{
    return mStatus == RpcResponseStatus::Success;
}
