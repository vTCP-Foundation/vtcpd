#ifndef VTCPD_MAXFLOWCALCULATIONSOURCESNDLEVELTRANSACTION_H
#define VTCPD_MAXFLOWCALCULATIONSOURCESNDLEVELTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../contractors/ContractorsManager.h"
#include "../../../equivalents/EquivalentsSubsystemsRouter.h"
#include "../../../network/messages/max_flow_calculation/MaxFlowCalculationSourceSndLevelMessage.h"
#include "../../../network/messages/max_flow_calculation/ResultMaxFlowCalculationMessage.h"
#include "../../../network/messages/max_flow_calculation/ResultMaxFlowCalculationGatewayMessage.h"
#include "../../../network/messages/max_flow_calculation/ExchangeRatesMessage.h"
#include "../../../rates/manager/ExchangeRatesManager.h"

class MaxFlowCalculationSourceSndLevelTransaction : public BaseTransaction
{

public:
    typedef shared_ptr<MaxFlowCalculationSourceSndLevelTransaction> Shared;

public:
    MaxFlowCalculationSourceSndLevelTransaction(
        MaxFlowCalculationSourceSndLevelMessage::Shared message,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        ExchangeRatesManager *exchangeRatesManager,
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

private:
    MaxFlowCalculationSourceSndLevelMessage::Shared mMessage;
    ContractorsManager *mContractorsManager;
    EquivalentsSubsystemsRouter *mEquivalentsSubsystemsRouter;
    ExchangeRatesManager *mExchangeRatesManager;
};


#endif //VTCPD_MAXFLOWCALCULATIONSOURCESNDLEVELTRANSACTION_H