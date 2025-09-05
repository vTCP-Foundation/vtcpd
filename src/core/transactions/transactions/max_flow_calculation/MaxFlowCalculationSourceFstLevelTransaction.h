#ifndef VTCPD_MAXFLOWCALCULATIONSOURCEFSTLEVELTRANSACTION_H
#define VTCPD_MAXFLOWCALCULATIONSOURCEFSTLEVELTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../contractors/ContractorsManager.h"
#include "../../../equivalents/EquivalentsSubsystemsRouter.h"
#include "../../../network/messages/max_flow_calculation/MaxFlowCalculationSourceFstLevelMessage.h"
#include "../../../network/messages/max_flow_calculation/MaxFlowCalculationSourceSndLevelMessage.h"
#include "../../../network/messages/max_flow_calculation/ResultMaxFlowCalculationGatewayMessage.h"
#include "../../../network/messages/max_flow_calculation/ExchangeRatesMessage.h"
#include "../../../rates/manager/ExchangeRatesManager.h"
#include "../../../rates/manager/CommissionsManager.h"

class MaxFlowCalculationSourceFstLevelTransaction : public BaseTransaction
{

public:
    typedef shared_ptr<MaxFlowCalculationSourceFstLevelTransaction> Shared;

public:
    MaxFlowCalculationSourceFstLevelTransaction(
        MaxFlowCalculationSourceFstLevelMessage::Shared message,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        ExchangeRatesManager *exchangeRatesManager,
        CommissionsManager *commissionsManager,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:
	void sendResultToInitiator(SerializedEquivalent equivalent);
	void sendCachedResultToInitiator(
		TopologyCache::Shared maxFlowCalculationCachePtr,
		SerializedEquivalent equivalent);
	void sendGatewayResultToInitiator(SerializedEquivalent equivalent);
    void sendCachedGatewayResultToInitiator(
        TopologyCache::Shared maxFlowCalculationCachePtr,
        SerializedEquivalent equivalent);
    void sendExchangeRatesIfNeeded();

    MaxFlowCalculationSourceFstLevelMessage::Shared mMessage;
    ContractorsManager *mContractorsManager;
    EquivalentsSubsystemsRouter *mEquivalentsSubsystemsRouter;
    ExchangeRatesManager *mExchangeRatesManager;
    CommissionsManager *mCommissionsManager;
};


#endif //VTCPD_MAXFLOWCALCULATIONSOURCEFSTLEVELTRANSACTION_H
