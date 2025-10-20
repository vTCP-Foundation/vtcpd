#ifndef VTCPD_RESOURCESMANAGER_H
#define VTCPD_RESOURCESMANAGER_H

#include "../../common/Types.h"
#include "../../contractors/addresses/BaseAddress.h"
#include "../../transactions/transactions/base/TransactionUUID.h"

#include "../resources/BaseResource.h"

#include "../../common/exceptions/ConflictError.h"

#include <boost/signals2.hpp>
#include <vector>

using namespace std;
namespace signals = boost::signals2;

class ResourcesManager
{
public:
    typedef signals::signal<void(
        const TransactionUUID&,
        BaseAddress::Shared,
        const SerializedEquivalent)>
    RequestPathsResourcesSignal;
    typedef signals::signal<void(
        const TransactionUUID&,
        BaseAddress::Shared,
        const vector<SerializedEquivalent>&,
        const SerializedEquivalent)>
    RequestExchangePathsResourceSignal;
    typedef signals::signal<void(const TransactionUUID&)> RequestObservingBlockNumberSignal;
    typedef signals::signal<void(BaseResource::Shared)> AttachResourceSignal;

public:
    void putResource(
        BaseResource::Shared resource);

    void requestPaths(
        const TransactionUUID &transactionUUID,
        BaseAddress::Shared contractorAddress,
        const SerializedEquivalent equivalent) const;

    void requestExchangePaths(
        const TransactionUUID &transactionUUID,
        BaseAddress::Shared contractorAddress,
        const vector<SerializedEquivalent> &exchangeEquivalents,
        const SerializedEquivalent receiverEquivalent) const;

    void requestObservingBlockNumber(
        const TransactionUUID &transactionUUID);

public:
    mutable RequestPathsResourcesSignal requestPathsResourcesSignal;
    mutable RequestExchangePathsResourceSignal requestExchangePathsResourceSignal;
    mutable RequestObservingBlockNumberSignal requestObservingBlockNumberSignal;
    mutable AttachResourceSignal attachResourceSignal;
};


#endif //VTCPD_RESOURCESMANAGER_H
