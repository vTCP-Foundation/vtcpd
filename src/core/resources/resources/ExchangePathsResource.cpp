#include "ExchangePathsResource.h"

ExchangePathsResource::ExchangePathsResource(
    const TransactionUUID &transactionUUID) :
    BaseResource(
        BaseResource::ExchangePaths,
        transactionUUID),
    mTransactionUUID(transactionUUID)
{}

const TransactionUUID& ExchangePathsResource::transactionUUID() const
{
    return mTransactionUUID;
}
