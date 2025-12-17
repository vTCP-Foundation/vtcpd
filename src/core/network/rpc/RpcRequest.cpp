#include "RpcRequest.h"


/**
 * Base request keeps the transaction UUID for later correlation.
 */
RpcRequest::RpcRequest(
    const TransactionUUID &transactionUUID) :
    mTransactionUUID(transactionUUID)
{
}

/**
 * Returns the transaction UUID attached to this request.
 */
const TransactionUUID& RpcRequest::transactionUUID() const
{
    return mTransactionUUID;
}
