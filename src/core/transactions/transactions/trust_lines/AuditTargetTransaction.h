#ifndef VTCPD_AUDITTARGETTRANSACTION_H
#define VTCPD_AUDITTARGETTRANSACTION_H

#include "base/BaseTrustLineTransaction.h"

#include "../../../topology/manager/TopologyTrustLinesManager.h"
#include "../../../topology/cache/TopologyCacheManager.h"
#include "../../../topology/cache/MaxFlowCacheManager.h"
#include "../../../io/storage/record/trust_line/TrustLineRecord.h"

class AuditTargetTransaction : public BaseTrustLineTransaction
{

public:
    typedef shared_ptr<AuditTargetTransaction> Shared;

public:
    AuditTargetTransaction(
        AuditMessage::Shared message,
        ContractorsManager *contractorsManager,
        TrustLinesManager *manager,
        StorageHandler *storageHandler,
        Keystore *keystore,
        TopologyTrustLinesManager *topologyTrustLinesManager,
        TopologyCacheManager *topologyCacheManager,
        MaxFlowCacheManager *maxFlowCacheManager,
        FeaturesManager *featuresManager,
        TrustLinesInfluenceController *trustLinesInfluenceController,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:
    // Extends base trust line stages with block number retrieval.
    enum Stages
    {
        Initialization = BaseTrustLineTransaction::Initialization,
        BlockNumberRequest = 7,
    };

    // Performs initial validation before starting block number request.
    TransactionResult::SharedConst runInitializationStage();

    // Switches to block number request stage.
    TransactionResult::SharedConst startBlockNumberRequest();

    // Requests block number and continues audit processing.
    TransactionResult::SharedConst runBlockNumberRequestStage();

    // Executes audit verification and state updates after block number retrieval.
    TransactionResult::SharedConst runAuditProcessingStage();

    // Sets trust line to Conflict state and persists the change.
    void setTrustLineToConflict();

    // Checks if any receipt exists for the specified transaction.
    bool hasAnyReceiptForTransaction(
        IOTransaction::Shared ioTransaction,
        const TransactionUUID &transactionUUID) const;

    // Loads effective claiming block number for a transaction.
    BlockNumber effectiveClaimingBlockNumber(
        IOTransaction::Shared ioTransaction,
        const TransactionUUID &transactionUUID) const;

    // Sends an audit response with optional transaction UUID list.
    TransactionResult::SharedConst sendAuditResponse(
        ConfirmationMessage::OperationState state,
        const vector<TransactionUUID> &transactionUUIDs = {});

    void setIncomingTrustLineAmount(
        IOTransaction::Shared ioTransaction);

    void closeOutgoingTrustLine(
        IOTransaction::Shared ioTransaction);

    void populateHistory(
        IOTransaction::Shared ioTransaction,
        TrustLineRecord::TrustLineOperationType operationType);

private:
    string mSenderIncomingIP;
    vector<BaseAddress::Shared> mContractorAddresses;

    AuditMessage::Shared mMessage;
    TopologyTrustLinesManager *mTopologyTrustLinesManager;
    TopologyCacheManager *mTopologyCacheManager;
    MaxFlowCacheManager *mMaxFlowCacheManager;

    // Tracks current block number for observing checks.
    BlockNumber mCurrentBlockNumber;
    // Prevents duplicate block number RPC requests.
    bool mBlockNumberRequestSent;
    // Stores normalized initiator transaction list for comparison.
    vector<TransactionUUID> mInitiatorTransactionList;

    TrustLineAmount mPreviousIncomingAmount;
    TrustLineAmount mPreviousOutgoingAmount;
    TrustLine::TrustLineState mPreviousState;
};


#endif //VTCPD_AUDITTARGETTRANSACTION_H
