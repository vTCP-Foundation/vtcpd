#ifndef VTCPD_COMPLETEDPAYMENTSOBSERVERMONITORINGTRANSACTION_H
#define VTCPD_COMPLETEDPAYMENTSOBSERVERMONITORINGTRANSACTION_H

#include "../../base/BaseTransaction.h"
#include "../../../../io/storage/interfaces/StorageHandler.h"
#include "../../../../crypto/keychain.h"

#include <map>
#include <utility>
#include <vector>

/**
 * Periodic transaction that checks observer claim statuses for completed payments
 * and submits participant vote signatures for observing claims.
 */
class CompletedPaymentsObserverMonitoringTransaction : public BaseTransaction
{
public:
    using Shared = shared_ptr<CompletedPaymentsObserverMonitoringTransaction>;
    using ClaimsList = vector<pair<TransactionUUID, BlockNumber>>;

public:
    /**
     * Maximum number of completed transactions processed per monitoring cycle.
     */
    static constexpr uint32_t kMaxTransactionsPerCycle = 100;

public:
    /**
     * Initializes monitoring transaction with storage and signing dependencies.
     */
    CompletedPaymentsObserverMonitoringTransaction(
        StorageHandler *storageHandler,
        crypto::Keystore *keystore,
        Logger &log);

    /**
     * Executes transaction state machine.
     */
    TransactionResult::SharedConst run() override;

    /**
     * Provides transaction-specific log prefix.
     */
    const string logHeader() const override;

protected:
    /**
     * State machine stages for observer monitoring.
     */
    enum Stages {
        GetCurrentBlock = 1,
        GetTransactionsForMonitoring,
        GetClaimStatuses,
        ProcessClaimStatuses,
        WaitForVotesSubmissionResponses
    };

protected:
    /**
     * Sends or processes GetBlockNumber RPC response.
     */
    TransactionResult::SharedConst runGetCurrentBlockStage();

    /**
     * Loads completed transactions and own public key from storage.
     */
    TransactionResult::SharedConst runGetTransactionsForMonitoringStage();

    /**
     * Sends or processes GetClaimStatuses RPC response.
     */
    TransactionResult::SharedConst runGetClaimStatusesStage();

    /**
     * Submits votes for observing claims and collects pending requests.
     */
    TransactionResult::SharedConst runProcessClaimStatusesStage();

    /**
     * Waits for SubmitClaimVotes RPC responses and completes transaction.
     */
    TransactionResult::SharedConst runWaitForVotesSubmissionResponsesStage();

    /**
     * Task 15-04: Serializes claim status request data for signing in canonical order.
     */
    pair<BytesShared, size_t> serializeGetClaimStatusesForSigning(
        const ClaimsList &claims,
        const sphincs::PublicKey::Shared &publicKey);

    /**
     * Task 15-04: Serializes vote submission data for signing in canonical order.
     */
    pair<BytesShared, size_t> serializeSubmitClaimVotesForSigning(
        const TransactionUUID &transactionUUID,
        BlockNumber maxClaimBlockNumber,
        const map<PaymentNodeID, sphincs::Signature::Shared> &votes,
        const sphincs::PublicKey::Shared &publicKey);

    /**
     * Validates that the next queued RPC response matches expected method.
     */
    bool hasExpectedRpcResponse(RpcMethod method) const;

    /**
     * Marks the next pending response as received and returns its claim UUID.
     */
    bool markNextPendingResponseReceived(TransactionUUID &claimUUID);

    /**
     * Logs completion marker for the monitoring transaction.
     */
    void logCompletion() const;

protected:
    /**
     * Provides access to database operations for monitoring.
     */
    StorageHandler *mStorageHandler;

    /**
     * Provides signing of serialized payloads.
     */
    crypto::Keystore *mKeystore;

    /**
     * Current block number retrieved from observer.
     */
    BlockNumber mCurrentBlockNumber;

    /**
     * Completed transactions eligible for monitoring.
     */
    ClaimsList mTransactionsForMonitoring;

    /**
     * Own SPHINCS public key cached for request signing.
     */
    sphincs::PublicKey::Shared mOwnPublicKey;

    /**
     * Observing claims selected for vote submission.
     */
    ClaimsList mObservingClaims;

    /**
     * Pending SubmitClaimVotes responses keyed by claim UUID.
     */
    map<TransactionUUID, bool> mPendingResponses;

    /**
     * Tracks whether GetBlockNumber request has been sent.
     */
    bool mCurrentBlockRequestSent;

    /**
     * Tracks whether GetClaimStatuses request has been sent.
     */
    bool mClaimStatusesRequestSent;
};

#endif // VTCPD_COMPLETEDPAYMENTSOBSERVERMONITORINGTRANSACTION_H
