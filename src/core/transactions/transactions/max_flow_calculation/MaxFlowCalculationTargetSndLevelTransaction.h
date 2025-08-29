#ifndef VTCPD_MAXFLOWCALCULATIONTARGETSNDLEVELTRANSACTION_H
#define VTCPD_MAXFLOWCALCULATIONTARGETSNDLEVELTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../contractors/ContractorsManager.h"
#include "../../../trust_lines/manager/TrustLinesManager.h"
#include "../../../topology/cache/TopologyCacheManager.h"
#include "../../../network/messages/max_flow_calculation/MaxFlowCalculationTargetSndLevelMessage.h"
#include "../../../network/messages/max_flow_calculation/ResultMaxFlowCalculationMessage.h"
#include "../../../network/messages/max_flow_calculation/ResultMaxFlowCalculationGatewayMessage.h"
#include "../../../network/messages/max_flow_calculation/ExchangeRatesMessage.h"
#include "../../../rates/manager/ExchangeRatesManager.h"

class MaxFlowCalculationTargetSndLevelTransaction : public BaseTransaction
{

public:
    typedef shared_ptr<MaxFlowCalculationTargetSndLevelTransaction> Shared;

public:
    MaxFlowCalculationTargetSndLevelTransaction(
        MaxFlowCalculationTargetSndLevelMessage::Shared message,
        ContractorsManager *contractorsManager,
        TrustLinesManager *manager,
        TopologyCacheManager *topologyCacheManager,
        ExchangeRatesManager *exchangeRatesManager,
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

    void sendExchangeRatesIfNeeded();

private:
    MaxFlowCalculationTargetSndLevelMessage::Shared mMessage;
    ContractorsManager *mContractorsManager;
    TrustLinesManager *mTrustLinesManager;
    TopologyCacheManager *mTopologyCacheManager;
    ExchangeRatesManager *mExchangeRatesManager;
    bool mIAmGateway;
};


#endif //VTCPD_MAXFLOWCALCULATIONTARGETSNDLEVELTRANSACTION_H
