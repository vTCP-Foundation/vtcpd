#ifndef VTCPD_AUDITSOURCETRANSACTION_H
#define VTCPD_AUDITSOURCETRANSACTION_H

#include "base/BaseTrustLineTransaction.h"

class AuditSourceTransaction : public BaseTrustLineTransaction
{

public:
    typedef shared_ptr<AuditSourceTransaction> Shared;

public:
    AuditSourceTransaction(
        ContractorID contractorID,
        const SerializedEquivalent equivalent,
        ContractorsManager *contractorsManager,
        TrustLinesManager *manager,
        StorageHandler *storageHandler,
        Keystore *keystore,
        FeaturesManager *featuresManager,
        TrustLinesInfluenceController *trustLinesInfluenceController,
        Logger &logger);

    AuditSourceTransaction(
        const SerializedEquivalent equivalent,
        ContractorsManager *contractorsManager,
        ContractorID contractorID,
        TrustLinesManager *manager,
        StorageHandler *storageHandler,
        Keystore *keystore,
        FeaturesManager *featuresManager,
        TrustLinesInfluenceController *trustLinesInfluenceController,
        Logger &logger);

    TransactionResult::SharedConst run() override;

protected:
    const string logHeader() const override;

private:
    // Extends base trust line stages with block number retrieval step.
    enum Stages
    {
        Initialization = BaseTrustLineTransaction::Initialization,
        NextAttempt = BaseTrustLineTransaction::NextAttempt,
        ResponseProcessing = BaseTrustLineTransaction::ResponseProcessing,
        Pending = BaseTrustLineTransaction::Pending,
        ContractorPending = BaseTrustLineTransaction::ContractorPending,
        NextAttemptPending = BaseTrustLineTransaction::NextAttemptPending,
        BlockNumberRequest = 7,
    };

    // Tracks which audit action should resume after block number retrieval.
    enum BlockNumberRequestPurpose
    {
        BlockNumberRequestNone = 0,
        BlockNumberRequestInitialization = 1,
        BlockNumberRequestNextAttempt = 2,
    };

    TransactionResult::SharedConst runInitializationStage();

    TransactionResult::SharedConst runAuditPendingStage();

    TransactionResult::SharedConst runNextAttemptStage();

    TransactionResult::SharedConst runNextAttemptAuditPendingStage();

    // Switches to block number stage for the specified audit flow.
    TransactionResult::SharedConst startBlockNumberRequest(
        BlockNumberRequestPurpose purpose);

    // Requests block number and resumes audit flow once response is received.
    TransactionResult::SharedConst runBlockNumberRequestStage();

    TransactionResult::SharedConst runResponseProcessingStage();

    TransactionResult::SharedConst runContractorPendingStage();

    // Initializes audit data and sends the first audit message.
    TransactionResult::SharedConst initializeAudit();

    // Resends audit message using stored signature for pending audits.
    TransactionResult::SharedConst nextAttemptAudit();

    // Sends an audit message with the current transaction list.
    TransactionResult::SharedConst sendAuditMessage();

    // Sets trust line state to Conflict and persists it.
    void setTrustLineToConflict();


private:
    uint16_t mCountSendingAttempts;
    uint16_t mCountPendingAttempts;
    uint16_t mCountContractorPendingAttempts;
    // Tracks current block number from observer for audit decisions.
    BlockNumber mCurrentBlockNumber;
    // Prevents duplicate block number RPC requests.
    bool mBlockNumberRequestSent;
    // Identifies which audit flow should resume after block retrieval.
    BlockNumberRequestPurpose mBlockNumberRequestPurpose;
    // Stores locally finalized transactions not finalized on contractor side.
    vector<TransactionUUID> mAsymmetricFinalizedTransactions;
    // Counts audit retries after update list response.
    uint8_t mAuditRetryCount;
    // Limits update list retries to a single attempt.
    static const uint8_t kMaxAuditRetries = 1;
};


#endif //VTCPD_AUDITSOURCETRANSACTION_H
