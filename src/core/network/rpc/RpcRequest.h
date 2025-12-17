#ifndef VTCPD_RPCREQUEST_H
#define VTCPD_RPCREQUEST_H

#include "../../transactions/transactions/base/TransactionUUID.h"
#include "RpcMethod.h"

#include <memory>

using namespace std;


class RpcRequest
{
public:
    typedef shared_ptr<RpcRequest> Shared;

public:
    /**
     * Stores the transaction identifier for downstream routing.
     */
    explicit RpcRequest(
        const TransactionUUID &transactionUUID);

    virtual ~RpcRequest() = default;

    /**
     * Provides access to the transaction identifier that originated the request.
     */
    const TransactionUUID& transactionUUID() const;

    /**
     * Each concrete request must declare which RPC method it represents.
     */
    virtual RpcMethod method() const = 0;

protected:
    TransactionUUID mTransactionUUID;
};


#endif // VTCPD_RPCREQUEST_H
