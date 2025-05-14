#ifndef VTCPD_MAXFLOWCALCULATIONFULLYTRANSACTION_H
#define VTCPD_MAXFLOWCALCULATIONFULLYTRANSACTION_H

#include "../base/BaseCollectTopologyTransaction.h"
#include "../../../interface/commands_interface/commands/max_flow_calculation/InitiateMaxFlowCalculationFullyCommand.h"

#include "CollectTopologyTransaction.h"

class MaxFlowCalculationFullyTransaction : public BaseCollectTopologyTransaction
{

public:
    typedef shared_ptr<MaxFlowCalculationFullyTransaction> Shared;

public:
    MaxFlowCalculationFullyTransaction(
        const InitiateMaxFlowCalculationFullyCommand::Shared command,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        TailManager *tailManager,
        Logger &logger,
		HopsCount_t hopsCount);

protected:
    const string logHeader() const override;

private:
    TransactionResult::SharedConst sendRequestForCollectingTopology() override;

    TransactionResult::SharedConst processCollectingTopology() override;

    TransactionResult::SharedConst applyCustomLogic() override;

    TrustLineAmount calculateMaxFlow(
        ContractorID contractorID);

    void calculateMaxFlowOnOneLevel();

    TrustLineAmount calculateOneNode(
        ContractorID nodeID,
        const TrustLineAmount &currentFlow,
        byte_t level);

    TransactionResult::SharedConst resultOk();

    TransactionResult::SharedConst resultProtocolError();

private:
    static const byte_t kMaxPathLength = 6;
    static const uint32_t kWaitMillisecondsForCalculatingMaxFlow = 4000;
    static const uint32_t kWaitMillisecondsForCalculatingMaxFlowAgain = 500;
    static const uint32_t kMaxWaitMillisecondsForCalculatingMaxFlow = 15000;
    static const uint16_t kCountRunningProcessCollectingTopologyStage =
        (kMaxWaitMillisecondsForCalculatingMaxFlow - kWaitMillisecondsForCalculatingMaxFlow) /
        kWaitMillisecondsForCalculatingMaxFlowAgain;

private:
    InitiateMaxFlowCalculationFullyCommand::Shared mCommand;
    vector<ContractorID> mForbiddenNodeIDs;
    byte_t mCurrentPathLength;
    TrustLineAmount mCurrentMaxFlow;
    ContractorID mCurrentContractor;
    size_t mCountProcessCollectingTopologyRun;
    TopologyTrustLinesManager::TrustLineWithPtrHashSet mFirstLevelTopology;
    vector<pair<ContractorID, BaseAddress::Shared>> mContractorIDs;
    vector<pair<ContractorID, TrustLineAmount>> mMaxFlows;
    size_t mCurrentGlobalContractorIdx;
    bool mIamGateway;
	HopsCount_t mHopsCnt;
};

#endif // VTCPD_MAXFLOWCALCULATIONFULLYTRANSACTION_H
