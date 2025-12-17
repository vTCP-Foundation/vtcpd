#ifndef VTCPD_RPCRESPONSESTATUS_H
#define VTCPD_RPCRESPONSESTATUS_H

/*
 * Status codes for RPC responses used to map transport and observer errors.
 */
enum class RpcResponseStatus {
    Success = 0,
    Timeout = 1,
    NetworkError = 2,
    ParseError = 3,
    RpcError = 4,
};

#endif // VTCPD_RPCRESPONSESTATUS_H
