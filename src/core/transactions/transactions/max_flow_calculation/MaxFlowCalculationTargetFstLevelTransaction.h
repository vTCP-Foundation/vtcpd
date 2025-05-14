#ifndef VTCPD_MAXFLOWCALCULATIONTARGETFSTLEVELTRANSACTION_H
#define VTCPD_MAXFLOWCALCULATIONTARGETFSTLEVELTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../contractors/ContractorsManager.h"
#include "../../../network/messages/max_flow_calculation/MaxFlowCalculationTargetFstLevelMessage.h"
#include "../../../network/messages/max_flow_calculation/MaxFlowCalculationTargetSndLevelMessage.h"
#include "../../../network/messages/max_flow_calculation/ResultMaxFlowCalculationGatewayMessage.h"
#include "../../../trust_lines/manager/TrustLinesManager.h"
#include "../../../topology/cache/TopologyCacheManager.h"

class MaxFlowCalculationTargetFstLevelTransaction : public BaseTransaction
{

public:
    typedef shared_ptr<MaxFlowCalculationTargetFstLevelTransaction> Shared;

public:
    MaxFlowCalculationTargetFstLevelTransaction(
        MaxFlowCalculationTargetFstLevelMessage::Shared message,
        ContractorsManager *contractorsManager,
        TrustLinesManager *manager,
        TopologyCacheManager *topologyCacheManager,
        Logger &logger,
        bool iAmGateway);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:

	void sendResultToInitiator();

    void sendCachedResultToInitiator(
        TopologyCache::Shared maxFlowCalculationCachePtr);

    void sendGatewayResultToInitiator();

    void sendCachedGatewayResultToInitiator(
        TopologyCache::Shared maxFlowCalculationCachePtr);

private:
    MaxFlowCalculationTargetFstLevelMessage::Shared mMessage;
    ContractorsManager *mContractorsManager;
    TrustLinesManager *mTrustLinesManager;
    TopologyCacheManager *mTopologyCacheManager;
    bool mIAmGateway;
};


#endif //VTCPD_MAXFLOWCALCULATIONTARGETFSTLEVELTRANSACTION_H
