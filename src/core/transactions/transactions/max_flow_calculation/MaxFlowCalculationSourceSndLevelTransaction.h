#ifndef VTCPD_MAXFLOWCALCULATIONSOURCESNDLEVELTRANSACTION_H
#define VTCPD_MAXFLOWCALCULATIONSOURCESNDLEVELTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../contractors/ContractorsManager.h"
#include "../../../trust_lines/manager/TrustLinesManager.h"
#include "../../../network/messages/max_flow_calculation/MaxFlowCalculationSourceSndLevelMessage.h"
#include "../../../network/messages/max_flow_calculation/ResultMaxFlowCalculationMessage.h"
#include "../../../network/messages/max_flow_calculation/ResultMaxFlowCalculationGatewayMessage.h"
#include "../../../network/messages/max_flow_calculation/ExchangeRatesMessage.h"
#include "../../../topology/cache/TopologyCacheManager.h"
#include "../../../rates/manager/ExchangeRatesManager.h"

class MaxFlowCalculationSourceSndLevelTransaction : public BaseTransaction
{

public:
    typedef shared_ptr<MaxFlowCalculationSourceSndLevelTransaction> Shared;

public:
    MaxFlowCalculationSourceSndLevelTransaction(
        MaxFlowCalculationSourceSndLevelMessage::Shared message,
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
    MaxFlowCalculationSourceSndLevelMessage::Shared mMessage;
    ContractorsManager *mContractorsManager;
    TrustLinesManager *mTrustLinesManager;
    TopologyCacheManager *mTopologyCacheManager;
    ExchangeRatesManager *mExchangeRatesManager;
    bool mIAmGateway;
};


#endif //VTCPD_MAXFLOWCALCULATIONSOURCESNDLEVELTRANSACTION_H
