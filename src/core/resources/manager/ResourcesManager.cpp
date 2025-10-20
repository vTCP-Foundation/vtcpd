#include "ResourcesManager.h"

void ResourcesManager::putResource(
    BaseResource::Shared resource)
{
    attachResourceSignal(
        resource);
}

void ResourcesManager::requestPaths(
    const TransactionUUID &transactionUUID,
    BaseAddress::Shared contractorAddress,
    const SerializedEquivalent equivalent) const
{
    requestPathsResourcesSignal(
        transactionUUID,
        contractorAddress,
        equivalent);
}

void ResourcesManager::requestExchangePaths(
    const TransactionUUID &transactionUUID,
    BaseAddress::Shared contractorAddress,
    const vector<SerializedEquivalent> &exchangeEquivalents,
    const SerializedEquivalent receiverEquivalent) const
{
    requestExchangePathsResourceSignal(
        transactionUUID,
        contractorAddress,
        exchangeEquivalents,
        receiverEquivalent);
}

void ResourcesManager::requestObservingBlockNumber(
    const TransactionUUID &transactionUUID)
{
    requestObservingBlockNumberSignal(
        transactionUUID);
}
