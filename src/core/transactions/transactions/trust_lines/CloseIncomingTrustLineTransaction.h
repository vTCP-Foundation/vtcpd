#ifndef VTCPD_CLOSEINCOMINGTRUSTLINETRANSACTION_H
#define VTCPD_CLOSEINCOMINGTRUSTLINETRANSACTION_H

#include "base/BaseTrustLineTransaction.h"
#include "../../../interface/commands_interface/commands/trust_lines/CloseIncomingTrustLineCommand.h"
#include "../../../network/messages/trust_lines/AuditMessage.h"
#include "../../../topology/manager/TopologyTrustLinesManager.h"
#include "../../../topology/cache/TopologyCacheManager.h"
#include "../../../topology/cache/MaxFlowCacheManager.h"
#include "../../../interface/events_interface/interface/EventsInterfaceManager.h"
#include "../../../subsystems_controller/SubsystemsController.h"

class CloseIncomingTrustLineTransaction : public BaseTrustLineTransaction
{

public:
    typedef shared_ptr<CloseIncomingTrustLineTransaction> Shared;

public:
    CloseIncomingTrustLineTransaction(
        CloseIncomingTrustLineCommand::Shared command,
        ContractorsManager *contractorsManager,
        TrustLinesManager *manager,
        StorageHandler *storageHandler,
        TopologyTrustLinesManager *topologyTrustLinesManager,
        TopologyCacheManager *topologyCacheManager,
        MaxFlowCacheManager *maxFlowCacheManager,
        SubsystemsController *subsystemsController,
        Keystore *keystore,
        FeaturesManager *featuresManager,
        EventsInterfaceManager *eventsInterfaceManager,
        TrustLinesInfluenceController *trustLinesInfluenceController,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    TransactionResult::SharedConst resultOK();

    TransactionResult::SharedConst resultForbiddenRun();

    TransactionResult::SharedConst resultProtocolError();

    TransactionResult::SharedConst resultKeysError();

    TransactionResult::SharedConst resultUnexpectedError();

protected:
    void populateHistory(
        IOTransaction::Shared ioTransaction,
        TrustLineRecord::TrustLineOperationType operationType);

    const string logHeader() const override;

private:
    // Extends base stages with block number retrieval.
    enum Stages
    {
        Initialization = BaseTrustLineTransaction::Initialization,
        Pending = BaseTrustLineTransaction::Pending,
        ResponseProcessing = BaseTrustLineTransaction::ResponseProcessing,
        ContractorPending = BaseTrustLineTransaction::ContractorPending,
        BlockNumberRequest = 7,
    };

    TransactionResult::SharedConst runInitializationStage();

    TransactionResult::SharedConst runAuditPendingStage();

    // Switches to block number request stage before audit.
    TransactionResult::SharedConst startBlockNumberRequest();

    // Requests block number and resumes audit once available.
    TransactionResult::SharedConst runBlockNumberRequestStage();

    TransactionResult::SharedConst runResponseProcessingStage();

    TransactionResult::SharedConst runContractorPendingStage();

    // Initializes audit data and sends audit message.
    TransactionResult::SharedConst initializeAudit();

private:
    CloseIncomingTrustLineCommand::Shared mCommand;
    TopologyTrustLinesManager *mTopologyTrustLinesManager;
    TopologyCacheManager *mTopologyCacheManager;
    MaxFlowCacheManager *mMaxFlowCacheManager;
    EventsInterfaceManager *mEventsInterfaceManager;
    SubsystemsController *mSubsystemsController;

    uint16_t mCountSendingAttempts;
    uint16_t mCountPendingAttempts;
    uint16_t mCountContractorPendingAttempts;

    TrustLineAmount mPreviousIncomingAmount;
    TrustLine::TrustLineState mPreviousState;

    // Stores current block number for observing checks.
    BlockNumber mCurrentBlockNumber;
    // Prevents duplicate block number RPC requests.
    bool mBlockNumberRequestSent;
    // Counts retries after update list response.
    uint8_t mAuditRetryCount;
    // Limits update list retries to a single attempt.
    static const uint8_t kMaxAuditRetries = 1;
};


#endif //VTCPD_CLOSEINCOMINGTRUSTLINETRANSACTION_H
