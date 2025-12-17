#ifndef VTCPD_GETBLOCKNUMBERRPCREQUEST_H
#define VTCPD_GETBLOCKNUMBERRPCREQUEST_H

#include "../RpcRequest.h"
#include "../../../../libs/json/json.h"

using json = nlohmann::json;


class GetBlockNumberRpcRequest : public RpcRequest
{
public:
    using Shared = shared_ptr<GetBlockNumberRpcRequest>;

public:
    /**
     * Constructs a request bound to the owning transaction UUID.
     */
    explicit GetBlockNumberRpcRequest(
        const TransactionUUID &transactionUUID);

    ~GetBlockNumberRpcRequest() override = default;

    /**
     * Identifies the RPC method represented by this request.
     */
    RpcMethod method() const override;

    /**
     * Serializes the request into JSON-RPC 1.0 payload expected by observer.
     */
    json toJson() const;
};

#endif // VTCPD_GETBLOCKNUMBERRPCREQUEST_H
