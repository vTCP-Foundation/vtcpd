#ifndef VTCPD_MAXFLOWCALCULATIONSOURCEFSTLEVELTRANSACTION_H
#define VTCPD_MAXFLOWCALCULATIONSOURCEFSTLEVELTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../contractors/ContractorsManager.h"
#include "../../../trust_lines/manager/TrustLinesManager.h"
#include "../../../network/messages/max_flow_calculation/MaxFlowCalculationSourceFstLevelMessage.h"
#include "../../../network/messages/max_flow_calculation/MaxFlowCalculationSourceSndLevelMessage.h"
#include "../../../network/messages/max_flow_calculation/ResultMaxFlowCalculationGatewayMessage.h"
#include "../../../network/messages/max_flow_calculation/ExchangeRatesMessage.h"
#include "../../../topology/cache/TopologyCacheManager.h"
#include "../../../rates/manager/ExchangeRatesManager.h"

class MaxFlowCalculationSourceFstLevelTransaction : public BaseTransaction
{

public:
    typedef shared_ptr<MaxFlowCalculationSourceFstLevelTransaction> Shared;

public:
    MaxFlowCalculationSourceFstLevelTransaction(
        MaxFlowCalculationSourceFstLevelMessage::Shared message,
        ContractorsManager *contractorsManager,
        TrustLinesManager *trustLinesManager,
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

    MaxFlowCalculationSourceFstLevelMessage::Shared mMessage;
    ContractorsManager *mContractorsManager;
    TrustLinesManager *mTrustLinesManager;
    TopologyCacheManager *mTopologyCacheManager;
    ExchangeRatesManager *mExchangeRatesManager;
    bool mIAmGateway;
};


#endif //VTCPD_MAXFLOWCALCULATIONSOURCEFSTLEVELTRANSACTION_H
