#ifndef VTCPD_COLLECTTOPOLOGYTRANSACTION_H
#define VTCPD_COLLECTTOPOLOGYTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../contractors/ContractorsManager.h"
#include "../../../trust_lines/manager/TrustLinesManager.h"
#include "../../../topology/manager/TopologyTrustLinesManager.h"
#include "../../../topology/cache/TopologyCacheManager.h"
#include "../../../topology/cache/MaxFlowCacheManager.h"

#include "../../../network/messages/max_flow_calculation/InitiateMaxFlowCalculationMessage.h"
#include "../../../network/messages/max_flow_calculation/MaxFlowCalculationSourceFstLevelMessage.h"

class CollectTopologyTransaction : public BaseTransaction
{

public:
    typedef shared_ptr<CollectTopologyTransaction> Shared;

public:
    CollectTopologyTransaction(
        const SerializedEquivalent equivalent,
        const vector<BaseAddress::Shared> &contractorAddresses,
        ContractorsManager *contractorsManager,
        TrustLinesManager *manager,
        TopologyTrustLinesManager *topologyTrustLineManager,
        TopologyCacheManager *topologyCacheManager,
        MaxFlowCacheManager *maxFlowCacheManager,
        bool iAmGateway,
        Logger &logger,
		HopsCount_t hopsCount);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:
    void sendMessagesToContractors();

    void sendMessagesOnFirstLevel();

    bool isNodeListedInTransactionContractors(
        BaseAddress::Shared nodeAddress) const;

private:
    ContractorsManager *mContractorsManager;
    TrustLinesManager *mTrustLinesManager;
    TopologyTrustLinesManager *mTopologyTrustLineManager;
    TopologyCacheManager *mTopologyCacheManager;
    MaxFlowCacheManager *mMaxFlowCacheManager;
    bool mIamGateway;
    // todo : use parameter from config, which should set the max length of payment path
    // 0 - points that coordinator will collect its neighbours for topology and wil not send message on its first level
    HopsCount_t mHopsCnt;

    vector<BaseAddress::Shared> mContractorAddresses;
};


#endif //VTCPD_COLLECTTOPOLOGYTRANSACTION_H
