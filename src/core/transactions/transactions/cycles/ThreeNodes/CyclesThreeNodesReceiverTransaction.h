#ifndef VTCPD_THREENODESRESPONSETRANSACTION_H
#define VTCPD_THREENODESRESPONSETRANSACTION_H

#include "../../base/BaseTransaction.h"
#include "../../../../contractors/ContractorsManager.h"
#include "../../../../trust_lines/manager/TrustLinesManager.h"
#include "../../../../io/storage/interfaces/StorageHandler.h"
#include "../../../../network/messages/cycles/ThreeNodes/CyclesThreeNodesBalancesRequestMessage.h"
#include "../../../../network/messages/cycles/ThreeNodes/CyclesThreeNodesBalancesResponseMessage.h"

#include <vector>

class CyclesThreeNodesReceiverTransaction :
    public BaseTransaction
{

public:
    CyclesThreeNodesReceiverTransaction(
        CyclesThreeNodesBalancesRequestMessage::Shared message,
        ContractorsManager *contractorsManager,
        TrustLinesManager *manager,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

protected:
    CyclesThreeNodesBalancesRequestMessage::Shared mRequestMessage;
    ContractorsManager *mContractorsManager;
    TrustLinesManager *mTrustLinesManager;
};
#endif //VTCPD_THREENODESRESPONSETRANSACTION_H
