#ifndef VTCPD_RPCRESPONSE_H
#define VTCPD_RPCRESPONSE_H

#include "../../transactions/transactions/base/TransactionUUID.h"
#include "RpcMethod.h"
#include "RpcResponseStatus.h"

#include <memory>
#include <string>

using namespace std;


class RpcResponse
{
public:
    typedef shared_ptr<RpcResponse> Shared;

public:
    /**
     * Captures the shared metadata for every RPC response.
     */
    RpcResponse(
        const TransactionUUID &transactionUUID,
        const RpcResponseStatus status,
        const string &errorMessage = "");

    virtual ~RpcResponse() = default;

    /**
     * Provides the originating transaction identifier.
     */
    const TransactionUUID& transactionUUID() const;

    /**
     * Exposes the transport or RPC level status.
     */
    RpcResponseStatus status() const;

    /**
     * Returns the descriptive error message when status is not Success.
     */
    const string& errorMessage() const;

    /**
     * Convenience flag to check if the call finished successfully.
     */
    bool isSuccess() const;

    /**
     * Each concrete response must report which RPC method it belongs to.
     */
    virtual RpcMethod method() const = 0;

protected:
    TransactionUUID mTransactionUUID;
    RpcResponseStatus mStatus;
    string mErrorMessage;
};


#endif // VTCPD_RPCRESPONSE_H
