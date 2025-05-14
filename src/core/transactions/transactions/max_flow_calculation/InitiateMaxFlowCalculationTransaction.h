#ifndef VTCPD_INITIATEMAXFLOWCALCULATIONTRANSACTION_H
#define VTCPD_INITIATEMAXFLOWCALCULATIONTRANSACTION_H

#include "../base/BaseCollectTopologyTransaction.h"
#include "../../../interface/commands_interface/commands/max_flow_calculation/InitiateMaxFlowCalculationCommand.h"

#include "../../../network/messages/max_flow_calculation/InitiateMaxFlowCalculationMessage.h"
#include "../../../network/messages/max_flow_calculation/MaxFlowCalculationSourceFstLevelMessage.h"
#include "../../../network/messages/max_flow_calculation/ResultMaxFlowCalculationMessage.h"

#include "CollectTopologyTransaction.h"

class InitiateMaxFlowCalculationTransaction : public BaseCollectTopologyTransaction
{

public:
    typedef shared_ptr<InitiateMaxFlowCalculationTransaction> Shared;

public:
    InitiateMaxFlowCalculationTransaction(
        InitiateMaxFlowCalculationCommand::Shared command,
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

    TransactionResult::SharedConst resultFinalOk();

    TransactionResult::SharedConst resultIntermediateOk();

    TransactionResult::SharedConst resultProtocolError();

private:
    static const byte_t kShortMaxPathLength = 5;
    static const byte_t kLongMaxPathLength = 6;
    static const uint32_t kWaitMillisecondsForCalculatingMaxFlow = 1000;
    static const uint32_t kWaitMillisecondsForCalculatingMaxFlowAgain = 500;
    static const uint32_t kMaxWaitMillisecondsForCalculatingMaxFlow = 10000;
    static const uint16_t kCountRunningProcessCollectingTopologyStage =
        (kMaxWaitMillisecondsForCalculatingMaxFlow - kWaitMillisecondsForCalculatingMaxFlow * 2) /
        kWaitMillisecondsForCalculatingMaxFlowAgain;

private:
    InitiateMaxFlowCalculationCommand::Shared mCommand;
    vector<ContractorID> mForbiddenNodeIDs;
    byte_t mCurrentPathLength;
    TrustLineAmount mCurrentMaxFlow;
    ContractorID mCurrentContractor;
    size_t mCountProcessCollectingTopologyRun;
    TopologyTrustLinesManager::TrustLineWithPtrHashSet mFirstLevelTopology;
    vector<pair<ContractorID, BaseAddress::Shared>> mContractorIDs;
    map<ContractorID, TrustLineAmount> mMaxFlows;
    uint16_t mResultStep;
    bool mShortMaxFlowsCalculated;
    bool mGatewayResponseProcessed;
    size_t mCurrentGlobalContractorIdx;
    bool mFinalTopologyCollected;
    byte_t mMaxPathLength;
    bool mIamGateway;
	HopsCount_t mHopsCnt;
};

#endif // VTCPD_INITIATEMAXFLOWCALCULATIONTRANSACTION_H
