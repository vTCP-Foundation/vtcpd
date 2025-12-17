#ifndef VTCPD_RPCMETHOD_H
#define VTCPD_RPCMETHOD_H

/*
 * List of supported RPC methods for async observer interaction.
 * Unknown is reserved for defensive handling of unexpected payloads.
 */
enum class RpcMethod {
    Unknown = 0,
    GetBlockNumber = 1,
    AcceptClaim = 2,
    GetClaimStatus = 3,
    SubmitClaimVotes = 4,
    GetClaimStatuses = 5,
};

#endif // VTCPD_RPCMETHOD_H
