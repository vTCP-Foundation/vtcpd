#ifndef VTCPD_SETOUTGOINGTRUSTLINETRANSACTION_H
#define VTCPD_SETOUTGOINGTRUSTLINETRANSACTION_H

#include "base/BaseTrustLineTransaction.h"
#include "../../../interface/commands_interface/commands/trust_lines/SetOutgoingTrustLineCommand.h"
#include "../../../topology/cache/TopologyCacheManager.h"
#include "../../../topology/cache/MaxFlowCacheManager.h"
#include "../../../interface/events_interface/interface/EventsInterfaceManager.h"
#include "../../../io/storage/record/trust_line/TrustLineRecord.h"
#include "../../../network/messages/trust_lines/AuditMessage.h"
#include "../../../subsystems_controller/SubsystemsController.h"


/**
 * This transaction is used to create/update/close the outgoing trust line for the remote contractor.
 *
 * In case if it would be launched against new one contractor with non zero amount -
 * then new one outgoing trust line would be created.
 *
 * In case if it would be launched against already present contractor, and transaction amount would not be zero -
 * then outgoing trust line to this contractor would be updated to the new amount.
 *
 * In case if it would be launched against already present contractor, and transaction amount WOULD BE zero -
 * then outgoing trust line to this contractor would be closed.
 */
class SetOutgoingTrustLineTransaction:
    public BaseTrustLineTransaction
{

public:
    typedef shared_ptr<SetOutgoingTrustLineTransaction> Shared;

public:
    SetOutgoingTrustLineTransaction(
        SetOutgoingTrustLineCommand::Shared command,
        ContractorsManager *contractorsManager,
        TrustLinesManager *manager,
        StorageHandler *storageHandler,
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

    TransactionResult::SharedConst resultContractorKeysError();

    TransactionResult::SharedConst resultUnexpectedError();

protected:
    void populateHistory(
        IOTransaction::Shared ioTransaction,
        TrustLineRecord::TrustLineOperationType operationType);

protected:
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
    SetOutgoingTrustLineCommand::Shared mCommand;
    TopologyCacheManager *mTopologyCacheManager;
    MaxFlowCacheManager *mMaxFlowCacheManager;
    EventsInterfaceManager *mEventsInterfaceManager;
    SubsystemsController *mSubsystemsController;

    uint16_t mCountSendingAttempts;
    uint16_t mCountPendingAttempts;
    uint16_t mCountContractorPendingAttempts;

    TrustLineAmount mPreviousOutgoingAmount;
    TrustLine::TrustLineState mPreviousState;
    TrustLinesManager::TrustLineOperationResult mOperationResult = TrustLinesManager::NoChanges;

    // Stores current block number for observing checks.
    BlockNumber mCurrentBlockNumber;
    // Prevents duplicate block number RPC requests.
    bool mBlockNumberRequestSent;
    // Stores locally finalized transactions not finalized on contractor side.
    vector<TransactionUUID> mAsymmetricFinalizedTransactions;
    // Counts retries after update list response.
    uint8_t mAuditRetryCount;
    // Limits update list retries to a single attempt.
    static const uint8_t kMaxAuditRetries = 1;
};


#endif //VTCPD_SETOUTGOINGTRUSTLINETRANSACTION_H
