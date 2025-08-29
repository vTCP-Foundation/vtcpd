#ifndef VTCPD_COLLECTTOPOLOGYFOREXCHANGETRANSACTION_H
#define VTCPD_COLLECTTOPOLOGYFOREXCHANGETRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../contractors/ContractorsManager.h"
#include "../../../topology/cache/TopologyCacheManager.h"
#include "../../../topology/cache/MaxFlowCacheManager.h"
#include "../../../equivalents/EquivalentsSubsystemsRouter.h"
#include "../../../rates/manager/ExchangeRatesManager.h"

#include "../../../network/messages/max_flow_calculation/InitiateMaxFlowForExchangeCalculationMessage.h"
#include "../../../network/messages/max_flow_calculation/MaxFlowCalculationSourceFstLevelMessage.h"

class CollectTopologyForExchangeTransaction : public BaseTransaction
{

public:
    typedef shared_ptr<CollectTopologyForExchangeTransaction> Shared;

public:
    CollectTopologyForExchangeTransaction(
        const SerializedEquivalent equivalent,
        const vector<SerializedEquivalent> &exchangeEquivalents,
        const vector<BaseAddress::Shared> &contractorAddresses,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        ExchangeRatesManager *exchangeRatesManager,
        TopologyCacheManager *topologyCacheManager,
        MaxFlowCacheManager *maxFlowCacheManager,
        Logger &logger,
        HopsCount_t hopsCount);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:
    void sendMessagesToContractors();

    void sendMessagesOnFirstLevel();

    bool isNodeListedInTransactionContractors(
        BaseAddress::Shared nodeAddress) const;

private:
    ContractorsManager *mContractorsManager;
    EquivalentsSubsystemsRouter *mEquivalentsSubsystemsRouter;
    ExchangeRatesManager *mExchangeRatesManager;
    TopologyCacheManager *mTopologyCacheManager;
    MaxFlowCacheManager *mMaxFlowCacheManager;
    HopsCount_t mHopsCnt;

    vector<BaseAddress::Shared> mContractorAddresses;
    vector<SerializedEquivalent> mExchangeEquivalents; // Sender's payment equivalents
};


#endif //VTCPD_COLLECTTOPOLOGYFOREXCHANGETRANSACTION_H