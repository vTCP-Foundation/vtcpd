#ifndef VTCPD_RECEIVEMAXFLOWCALCULATIONFOREXCHANGEONTARGETTRANSACTION_H
#define VTCPD_RECEIVEMAXFLOWCALCULATIONFOREXCHANGEONTARGETTRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../contractors/ContractorsManager.h"
#include "../../../trust_lines/manager/TrustLinesManager.h"
#include "../../../network/messages/max_flow_calculation/InitiateMaxFlowForExchangeCalculationMessage.h"
#include "../../../network/messages/max_flow_calculation/ResultMaxFlowCalculationMessage.h"
#include "../../../network/messages/max_flow_calculation/MaxFlowCalculationTargetFstLevelMessage.h"
#include "../../../topology/cache/TopologyCacheManager.h"

class ReceiveMaxFlowCalculationForExchangeOnTargetTransaction : public BaseTransaction
{

public:
    typedef shared_ptr<ReceiveMaxFlowCalculationForExchangeOnTargetTransaction> Shared;

public:
    ReceiveMaxFlowCalculationForExchangeOnTargetTransaction(
        InitiateMaxFlowForExchangeCalculationMessage::Shared message,
        ContractorsManager *contractorsManager,
        TrustLinesManager *trustLinesManager,
        TopologyCacheManager *topologyCacheManager,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:
    void sendMessagesOnFirstLevel();

    void sendResultToInitiator();

    void sendCachedResultToInitiator(
        TopologyCache::Shared maxFlowCalculationCachePtr);

private:
    InitiateMaxFlowForExchangeCalculationMessage::Shared mMessage;
    vector<SerializedEquivalent> mExchangeEquivalents; // Payer-provided set
    ContractorsManager *mContractorsManager;
    TrustLinesManager *mTrustLinesManager;
    TopologyCacheManager *mTopologyCacheManager;
};


#endif //VTCPD_RECEIVEMAXFLOWCALCULATIONFOREXCHANGEONTARGETTRANSACTION_H