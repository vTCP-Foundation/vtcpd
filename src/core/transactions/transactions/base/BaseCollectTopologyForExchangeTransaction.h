#ifndef VTCPD_BASECOLLECTTOPOLOGYFOREXCHANGETRANSACTION_H
#define VTCPD_BASECOLLECTTOPOLOGYFOREXCHANGETRANSACTION_H

#include "BaseTransaction.h"

#include "../../../equivalents/EquivalentsSubsystemsRouter.h"
#include "../../../rates/manager/ExchangeRatesManager.h"

#include "../../../network/messages/max_flow_calculation/ResultMaxFlowCalculationMessage.h"
#include "../../../network/messages/max_flow_calculation/ResultMaxFlowCalculationGatewayMessage.h"
#include "../../../network/messages/max_flow_calculation/ExchangeRatesMessage.h"
#include "../../../network/communicator/internal/incoming/TailManager.h"

class BaseCollectTopologyForExchangeTransaction : public BaseTransaction
{

public:
    typedef shared_ptr<BaseCollectTopologyForExchangeTransaction> Shared;

public:
    BaseCollectTopologyForExchangeTransaction(
        const TransactionType type,
        const SerializedEquivalent equivalent,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        ExchangeRatesManager *exchangeRatesManager,
        TailManager *tailManager,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    enum Stages
    {
        SendRequestForCollectingTopology = 1,
        ProcessCollectingTopology = 2,
        CustomLogic = 3
    };

protected:
    virtual TransactionResult::SharedConst sendRequestForCollectingTopology() = 0;

    virtual TransactionResult::SharedConst processCollectingTopology() = 0;

    virtual TransactionResult::SharedConst applyCustomLogic()
    {
        return resultDone();
    };

    void fillTopology();

    void fillRates();

protected:
    const uint16_t kFinalStep = 10;

protected:
    ContractorsManager *mContractorsManager;
    EquivalentsSubsystemsRouter *mEquivalentsSubsystemsRouter;
    ExchangeRatesManager *mExchangeRatesManager;
    set<ContractorID> mGateways;
    TailManager *mTailManager;
};


#endif //VTCPD_BASECOLLECTTOPOLOGYFOREXCHANGETRANSACTION_H