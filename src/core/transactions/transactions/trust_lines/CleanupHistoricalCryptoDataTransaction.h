#ifndef VTCPD_CLEANUPHISTORICALCRYPTODATATRANSACTION_H
#define VTCPD_CLEANUPHISTORICALCRYPTODATATRANSACTION_H

#include "../base/BaseTransaction.h"
#include "../../../contractors/ContractorsManager.h"
#include "../../../trust_lines/manager/TrustLinesManager.h"
#include "../../../io/storage/interfaces/StorageHandler.h"

/**
 * Transaction that removes obsolete historical crypto data after audit completion.
 */
class CleanupHistoricalCryptoDataTransaction : public BaseTransaction
{
public:
    typedef shared_ptr<CleanupHistoricalCryptoDataTransaction> Shared;

    /**
     * Initializes cleanup transaction for provided contractor and equivalent.
     */
    CleanupHistoricalCryptoDataTransaction(
        ContractorID contractorID,
        const SerializedEquivalent equivalent,
        ContractorsManager *contractorsManager,
        TrustLinesManager *trustLinesManager,
        StorageHandler *storageHandler,
        Logger &logger);

    /**
     * Runs transaction according to internal stage state.
     */
    TransactionResult::SharedConst run() override;

protected:
    /**
     * Returns log prefix for the transaction.
     */
    const string logHeader() const override;

private:
    /**
     * Loads trust line and audit data, computes threshold for cleanup.
     */
    TransactionResult::SharedConst runInitializationStage();

    /**
     * Requests current block number from observer and processes the response.
     */
    TransactionResult::SharedConst runBlockNumberRequestStage();

    /**
     * Iterates candidate audits and performs cleanup when safe.
     */
    TransactionResult::SharedConst runCleanupStage();

    /**
     * Handles cleanup for a single audit; returns false if cleanup must stop.
     */
    bool processAuditCleanup(
        const TrustLineID trustLineID,
        const AuditNumber auditNumber,
        const BlockNumber currentBlockNumber);

    /**
     * Checks whether a transaction UUID is referenced by any receipt records.
     */
    bool isTransactionReferenced(
        IOTransaction::Shared ioTransaction,
        const TransactionUUID &transactionUUID) const;

private:
    // Number of latest audits to keep before cleanup starts.
    static const uint8_t RETENTION_OFFSET = 2;

    // Processing stages for the cleanup transaction.
    enum Stages {
        Initialization = 1,
        BlockNumberRequest = 2,
        Cleanup = 3,
    };

    // Contractor for which historical data will be cleaned.
    ContractorID mContractorID;
    // Provides contractor data for validation.
    ContractorsManager *mContractorsManager;
    // Provides trust line data to resolve trust line ID.
    TrustLinesManager *mTrustLinesManager;
    // Storage handler used for IO transactions.
    StorageHandler *mStorageHandler;

    // Cached trust line ID for cleanup work.
    TrustLineID mTrustLineID;
    // Calculated audit threshold for cleanup.
    AuditNumber mThresholdAuditNumber;
    // Tracks whether block number request has been sent.
    bool mBlockNumberRequestSent;
    // Current block number retrieved from observer.
    BlockNumber mCurrentBlockNumber;
};

#endif //VTCPD_CLEANUPHISTORICALCRYPTODATATRANSACTION_H
