#ifndef VTCPD_GETBLOCKNUMBERRPCRESPONSE_H
#define VTCPD_GETBLOCKNUMBERRPCRESPONSE_H

#include "../RpcResponse.h"
#include "../../../common/Types.h"
#include "../../../../libs/json/json.h"

using json = nlohmann::json;


class GetBlockNumberRpcResponse : public RpcResponse
{
public:
    using Shared = shared_ptr<GetBlockNumberRpcResponse>;

public:
    /**
     * Captures response metadata and block number returned by observer.
     */
    GetBlockNumberRpcResponse(
        const TransactionUUID &transactionUUID,
        RpcResponseStatus status,
        BlockNumber blockNumber = 0,
        const string &errorMessage = "");

    ~GetBlockNumberRpcResponse() override = default;

    /**
     * Identifies this response with corresponding RPC method.
     */
    RpcMethod method() const override;

    /**
     * Provides access to received block number; 0 when unavailable.
     */
    BlockNumber blockNumber() const;

    /**
     * Parses JSON payload into typed response; maps parse failures to ParseError.
     */
    static Shared fromJson(
        const TransactionUUID &transactionUUID,
        RpcResponseStatus status,
        const json &responseJson,
        const string &errorMessage = "");

private:
    BlockNumber mBlockNumber;
};

#endif // VTCPD_GETBLOCKNUMBERRPCRESPONSE_H
