#ifndef VTCPD_MAXFLOWCALCULATIONTARGETFSTLEVELTRANSACTION_H
#define VTCPD_MAXFLOWCALCULATIONTARGETFSTLEVELTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../contractors/ContractorsManager.h"
#include "../../../equivalents/EquivalentsSubsystemsRouter.h"
#include "../../../network/messages/max_flow_calculation/MaxFlowCalculationTargetFstLevelMessage.h"
#include "../../../network/messages/max_flow_calculation/MaxFlowCalculationTargetSndLevelMessage.h"
#include "../../../network/messages/max_flow_calculation/ResultMaxFlowCalculationGatewayMessage.h"
#include "../../../network/messages/max_flow_calculation/ExchangeRatesMessage.h"
#include "../../../rates/manager/ExchangeRatesManager.h"

class MaxFlowCalculationTargetFstLevelTransaction : public BaseTransaction
{

public:
    typedef shared_ptr<MaxFlowCalculationTargetFstLevelTransaction> Shared;

public:
    MaxFlowCalculationTargetFstLevelTransaction(
        MaxFlowCalculationTargetFstLevelMessage::Shared message,
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
    MaxFlowCalculationTargetFstLevelMessage::Shared mMessage;
    ContractorsManager *mContractorsManager;
    EquivalentsSubsystemsRouter *mEquivalentsSubsystemsRouter;
    ExchangeRatesManager *mExchangeRatesManager;
};


#endif //VTCPD_MAXFLOWCALCULATIONTARGETFSTLEVELTRANSACTION_H
