#ifndef VTCPD_INITIATEMAXFLOWEXCHANGECALCULATIONTRANSACTION_H
#define VTCPD_INITIATEMAXFLOWEXCHANGECALCULATIONTRANSACTION_H

#include "../base/BaseCollectTopologyForExchangeTransaction.h"
#include "../../../interface/commands_interface/commands/max_flow_calculation/InitiateMaxFlowExchangeCalculationCommand.h"

#include "../../../network/messages/max_flow_calculation/InitiateMaxFlowForExchangeCalculationMessage.h"
#include "../../../network/messages/max_flow_calculation/MaxFlowCalculationSourceFstLevelMessage.h"
#include "../../../network/messages/max_flow_calculation/ResultMaxFlowCalculationMessage.h"

#include "CollectTopologyForExchangeTransaction.h"

// Data structures for exchange path handling
struct ExchangeStep {
    ContractorID nodeID;
    SerializedEquivalent fromEquivalent;
    SerializedEquivalent toEquivalent;
    TrustLineAmount exchangeRate; // raw integer rate without shift
    int16_t exchangeRateShift{0};
    TrustLineAmount minExchangeAmount{0};
    TrustLineAmount maxExchangeAmount{0};
    TrustLineAmount commission;
};

struct ExchangePath {
    vector<ContractorID> nodes;
    vector<SerializedEquivalent> equivalents;
    vector<ExchangeStep> exchangeSteps;
    TrustLineAmount minCapacity;
    double effectiveExchangeRate;
    TrustLineAmount totalCommissions;
    
    bool isValid() const { return !nodes.empty() && nodes.size() == equivalents.size(); }
    TrustLineAmount calculateMaxCapacity() const { return minCapacity; }
    double calculateEffectiveExchangeRate() const { return effectiveExchangeRate; }
    TrustLineAmount sumFixedCommissions() const { return totalCommissions; }
    bool startsWithEquivalent(SerializedEquivalent equiv) const { 
        return !equivalents.empty() && equivalents[0] == equiv; 
    }
};

struct OptimalPathResult {
    ExchangePath path;
    TrustLineAmount optimal_flow;
    TrustLineAmount received_amount;
    double effective_exchange_rate;
    double path_efficiency;
};

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

    // Path enumeration and LP methods
    vector<ExchangePath> enumerateAllFeasiblePaths(ContractorID targetContractor);
    
    void enumeratePathsFromEquivalent(
        SerializedEquivalent startEquivalent,
        ContractorID targetContractor,
        vector<ExchangePath> &allPaths,
        int maxPathLength = 6);
        
    void dfsEnumeratePaths(
        ContractorID currentNode,
        SerializedEquivalent currentEquivalent,
        ContractorID targetNode,
        SerializedEquivalent targetEquivalent,
        vector<ContractorID> &currentPath,
        vector<SerializedEquivalent> &currentEquivPath,
        vector<ExchangeStep> &currentExchanges,
        vector<ExchangePath> &results,
        int maxDepth,
        int currentDepth = 0);

    // removed legacy formatter; use external helper in cpp

    // Helper methods
    TrustLineAmount getSenderBalance(SerializedEquivalent equivalent);

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
    size_t mCountProcessCollectingTopologyRun;
    vector<pair<ContractorID, BaseAddress::Shared>> mContractorIDs;
    map<ContractorID, TrustLineAmount> mMaxFlows;
    map<ContractorID, vector<OptimalPathResult>> mOptimalPathResults;
    HopsCount_t mHopsCnt;
};

#endif // VTCPD_INITIATEMAXFLOWEXCHANGECALCULATIONTRANSACTION_H
