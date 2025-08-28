#ifndef VTCPD_BASECOLLECTTOPOLOGYTRANSACTION_H
#define VTCPD_BASECOLLECTTOPOLOGYTRANSACTION_H

#include "BaseTransaction.h"

#include "../../../equivalents/EquivalentsSubsystemsRouter.h"

#include "../../../network/messages/max_flow_calculation/ResultMaxFlowCalculationMessage.h"
#include "../../../network/messages/max_flow_calculation/ResultMaxFlowCalculationGatewayMessage.h"
#include "../../../network/communicator/internal/incoming/TailManager.h"

class BaseCollectTopologyTransaction : public BaseTransaction
{

public:
    typedef shared_ptr<BaseCollectTopologyTransaction> Shared;

public:
    BaseCollectTopologyTransaction(
        const TransactionType type,
        const SerializedEquivalent equivalent,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        TailManager *tailManager,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    enum Stages
    {
        SendRequestForCollectingTopology = 1,
        ProcessCollectingTopology = 2,
        CustomLogic = 3
    };

protected:
    virtual TransactionResult::SharedConst sendRequestForCollectingTopology() = 0;

    virtual TransactionResult::SharedConst processCollectingTopology() = 0;

    virtual TransactionResult::SharedConst applyCustomLogic()
    {
        return resultDone();
    };

    void fillTopology();

protected:
    const uint16_t kFinalStep = 10;

protected:
    ContractorsManager *mContractorsManager;
    EquivalentsSubsystemsRouter *mEquivalentsSubsystemsRouter;
    TrustLinesManager *mTrustLinesManager;
    TopologyTrustLinesManager *mTopologyTrustLineManager;
    TopologyCacheManager *mTopologyCacheManager;
    MaxFlowCacheManager *mMaxFlowCacheManager;
    set<ContractorID> mGateways;
    TailManager *mTailManager;
};


#endif //VTCPD_BASECOLLECTTOPOLOGYTRANSACTION_H
