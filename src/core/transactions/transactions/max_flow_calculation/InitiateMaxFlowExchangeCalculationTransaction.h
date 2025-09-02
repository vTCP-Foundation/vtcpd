#ifndef VTCPD_INITIATEMAXFLOWEXCHANGECALCULATIONTRANSACTION_H
#define VTCPD_INITIATEMAXFLOWEXCHANGECALCULATIONTRANSACTION_H

#include "../base/BaseCollectTopologyForExchangeTransaction.h"
#include "../../../interface/commands_interface/commands/max_flow_calculation/InitiateMaxFlowExchangeCalculationCommand.h"

#include "../../../network/messages/max_flow_calculation/InitiateMaxFlowForExchangeCalculationMessage.h"
#include "../../../network/messages/max_flow_calculation/MaxFlowCalculationSourceFstLevelMessage.h"
#include "../../../network/messages/max_flow_calculation/ResultMaxFlowCalculationMessage.h"

#include "CollectTopologyForExchangeTransaction.h"

class InitiateMaxFlowExchangeCalculationTransaction : public BaseCollectTopologyForExchangeTransaction
{

public:
    typedef shared_ptr<InitiateMaxFlowExchangeCalculationTransaction> Shared;

public:
    InitiateMaxFlowExchangeCalculationTransaction(
        InitiateMaxFlowExchangeCalculationCommand::Shared command,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        ExchangeRatesManager *exchangeRatesManager,
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
        (kMaxWaitMillisecondsForCalculatingMaxFlow - kWaitMillisecondsForCalculatingMaxFlow * 2) /
        kWaitMillisecondsForCalculatingMaxFlowAgain;

private:
    InitiateMaxFlowExchangeCalculationCommand::Shared mCommand;
    vector<SerializedEquivalent> mExchangeEquivalents;
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
    HopsCount_t mHopsCnt;
};

#endif // VTCPD_INITIATEMAXFLOWEXCHANGECALCULATIONTRANSACTION_H