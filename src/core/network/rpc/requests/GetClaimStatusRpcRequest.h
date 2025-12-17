#ifndef VTCPD_GETCLAIMSTATUSRPCREQUEST_H
#define VTCPD_GETCLAIMSTATUSRPCREQUEST_H

#include "../RpcRequest.h"
#include "../../../common/Types.h"
#include "../../../../libs/json/json.h"

using json = nlohmann::json;


class GetClaimStatusRpcRequest : public RpcRequest
{
public:
    using Shared = shared_ptr<GetClaimStatusRpcRequest>;

public:
    /**
     * Captures parameters for claim status request.
     */
    GetClaimStatusRpcRequest(
        const TransactionUUID &transactionUUID,
        const TransactionUUID &claimTransactionUUID,
        BlockNumber maxClaimBlockNumber);

    ~GetClaimStatusRpcRequest() override = default;

    /**
     * Indicates the RPC method for routing.
     */
    RpcMethod method() const override;

    /**
     * Returns the target claim UUID requested from observer.
     */
    const TransactionUUID& claimTransactionUUID() const;

    /**
     * Returns maximal allowed block number for claim.
     */
    BlockNumber maxClaimBlockNumber() const;

    /**
     * Serializes payload following observer contract.
     */
    json toJson() const;

private:
    TransactionUUID mClaimTransactionUUID;
    BlockNumber mMaxClaimBlockNumber;
};

#endif // VTCPD_GETCLAIMSTATUSRPCREQUEST_H
