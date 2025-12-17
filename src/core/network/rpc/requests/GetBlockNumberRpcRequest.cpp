#include "GetBlockNumberRpcRequest.h"

#include <boost/uuid/uuid_io.hpp>

namespace {
// JSON-RPC id is fixed to 1 per protocol examples in observer README.
constexpr int kJsonRpcId = 1;
constexpr char kMethodName[] = "RPCService.GetCurrentBlock";
}

GetBlockNumberRpcRequest::GetBlockNumberRpcRequest(
    const TransactionUUID &transactionUUID) :
    RpcRequest(transactionUUID)
{}

RpcMethod GetBlockNumberRpcRequest::method() const
{
    return RpcMethod::GetBlockNumber;
}

json GetBlockNumberRpcRequest::toJson() const
{
    // Single empty object param per observer protocol.
    json::array_t params = {json::object()};
    return json{
        {"method", kMethodName},
        {"params", params},
        {"id", kJsonRpcId}
    };
}
