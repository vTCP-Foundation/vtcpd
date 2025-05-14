#ifndef VTCPD_FINDPATHBYMAXFLOWTRANSACTION_H
#define VTCPD_FINDPATHBYMAXFLOWTRANSACTION_H

#include "../base/BaseCollectTopologyTransaction.h"
#include "../../../paths/PathsManager.h"
#include "../../../resources/manager/ResourcesManager.h"
#include "../../../resources/resources/PathsResource.h"

#include "../max_flow_calculation/CollectTopologyTransaction.h"

class FindPathByMaxFlowTransaction : public BaseCollectTopologyTransaction
{

public:
    typedef shared_ptr<FindPathByMaxFlowTransaction> Shared;

public:
    FindPathByMaxFlowTransaction(
        BaseAddress::Shared contractorAddress,
        const TransactionUUID &requestedTransactionUUID,
        const SerializedEquivalent equivalent,
        ContractorsManager *contractorsManager,
        ResourcesManager *resourcesManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        TailManager *tailManager,
        Logger &logger,
		HopsCount_t hopsCount);

protected:
    const string logHeader() const override;

private:
    TransactionResult::SharedConst sendRequestForCollectingTopology() override;

    TransactionResult::SharedConst processCollectingTopologyShortly()
    {
        return resultDone();
    }

    TransactionResult::SharedConst processCollectingTopology() override;

private:
    // ToDo: move to separate config file
    static const uint32_t kTopologyCollectingMillisecondsTimeout = 300;

    static const uint32_t kTopologyCollectingAgainMillisecondsTimeout = 200;
    static const uint32_t kMaxTopologyCollectingMillisecondsTimeout = 3000;
    static const uint16_t kCountRunningProcessCollectingTopologyStage =
        (kMaxTopologyCollectingMillisecondsTimeout - kTopologyCollectingMillisecondsTimeout) /
        kTopologyCollectingAgainMillisecondsTimeout;

private:
    ContractorID mContractorID;
    BaseAddress::Shared mContractorAddress;
    TransactionUUID mRequestedTransactionUUID;
    PathsManager *mPathsManager;
    ResourcesManager *mResourcesManager;
    size_t mCountProcessCollectingTopologyRun;
    bool mIamGateway;
	HopsCount_t mHopsCnt;
};


#endif //VTCPD_FINDPATHBYMAXFLOWTRANSACTION_H
