#ifndef VTCPD_MAXFLOWCALCULATIONTARGETSNDLEVELTRANSACTION_H
#define VTCPD_MAXFLOWCALCULATIONTARGETSNDLEVELTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../contractors/ContractorsManager.h"
#include "../../../equivalents/EquivalentsSubsystemsRouter.h"
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
    MaxFlowCalculationTargetSndLevelMessage::Shared mMessage;
    ContractorsManager *mContractorsManager;
    EquivalentsSubsystemsRouter *mEquivalentsSubsystemsRouter;
    ExchangeRatesManager *mExchangeRatesManager;
};


#endif //VTCPD_MAXFLOWCALCULATIONTARGETSNDLEVELTRANSACTION_H
