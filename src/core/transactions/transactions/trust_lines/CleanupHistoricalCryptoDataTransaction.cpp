#include "CleanupHistoricalCryptoDataTransaction.h"

#include "../../../network/rpc/requests/GetBlockNumberRpcRequest.h"
#include "../../../network/rpc/responses/GetBlockNumberRpcResponse.h"
#include "../../../common/exceptions/NotFoundError.h"

#include <algorithm>
#include <cstring>
#include <exception>
#include <set>
#include <sstream>

namespace {
/**
 * Default placeholders for uninitialized cleanup fields.
 */
constexpr TrustLineID kInvalidTrustLineId = 0;
constexpr AuditNumber kInitialAuditThreshold = 0;
constexpr BlockNumber kInitialBlockNumber = 0;
constexpr int64_t kNoCleanupThreshold = 0;

/**
 * Orders TransactionUUID values by raw bytes to ensure deterministic processing.
 */
bool compareTransactionUUID(
    const TransactionUUID &left,
    const TransactionUUID &right)
{
    return memcmp(
        left.data,
        right.data,
        TransactionUUID::kBytesSize) < 0;
}
}

CleanupHistoricalCryptoDataTransaction::CleanupHistoricalCryptoDataTransaction(
    ContractorID contractorID,
    const SerializedEquivalent equivalent,
    ContractorsManager *contractorsManager,
    TrustLinesManager *trustLinesManager,
    StorageHandler *storageHandler,
    Logger &logger) :
    BaseTransaction(
        BaseTransaction::CleanupHistoricalCryptoDataType,
        equivalent,
        logger),
    mContractorID(contractorID),
    mContractorsManager(contractorsManager),
    mTrustLinesManager(trustLinesManager),
    mStorageHandler(storageHandler),
    mTrustLineID(kInvalidTrustLineId),
    mThresholdAuditNumber(kInitialAuditThreshold),
    mBlockNumberRequestSent(false),
    mCurrentBlockNumber(kInitialBlockNumber)
{
    mStep = Stages::Initialization;
}

TransactionResult::SharedConst CleanupHistoricalCryptoDataTransaction::run()
{
    switch (mStep) {
    case Stages::Initialization:
        return runInitializationStage();
    case Stages::BlockNumberRequest:
        return runBlockNumberRequestStage();
    case Stages::Cleanup:
        return runCleanupStage();
    default:
        warning() << "Unknown stage " << mStep << ", finishing cleanup transaction";
        return resultDone();
    }
}

TransactionResult::SharedConst CleanupHistoricalCryptoDataTransaction::runInitializationStage()
{
    info() << "runInitializationStage. Contractor " << mContractorID;

    if (mContractorsManager == nullptr || mTrustLinesManager == nullptr || mStorageHandler == nullptr) {
        error() << "Missing dependencies for cleanup transaction";
        return resultDone();
    }

    if (!mContractorsManager->contractorPresent(mContractorID)) {
        warning() << "Contractor not found, skipping cleanup";
        return resultDone();
    }

    try {
        mTrustLineID = mTrustLinesManager->trustLineID(mContractorID);
    } catch (const NotFoundError &e) {
        warning() << "Trust line not found for cleanup. Details: " << e.what();
        return resultDone();
    }

    IOTransaction::Shared ioTransaction = nullptr;
    try {
        ioTransaction = mStorageHandler->beginTransaction();

        const auto currentAuditNumber =
            ioTransaction->auditHandler()->getActualAuditNumber(mTrustLineID);
        info() << "Current audit number: " << currentAuditNumber;

        const int64_t thresholdCandidate =
            static_cast<int64_t>(currentAuditNumber) - RETENTION_OFFSET;
        if (thresholdCandidate <= kNoCleanupThreshold) {
            info() << "Cleanup threshold is not reached, nothing to remove";
            return resultDone();
        }

        mThresholdAuditNumber = static_cast<AuditNumber>(thresholdCandidate);
        info() << "Cleanup threshold audit number: " << mThresholdAuditNumber;
    } catch (const exception &e) {
        if (ioTransaction != nullptr) {
            ioTransaction->rollback();
        }
        error() << "Failed to calculate cleanup threshold. Details: " << e.what();
        return resultDone();
    }

    // Proceed to block number request stage.
    mStep = Stages::BlockNumberRequest;
    return runBlockNumberRequestStage();
}

TransactionResult::SharedConst CleanupHistoricalCryptoDataTransaction::runBlockNumberRequestStage()
{
    info() << "runBlockNumberRequestStage";

    // Send block number request once and wait for response.
    if (!mBlockNumberRequestSent) {
        sendRpcRequest(
            make_shared<GetBlockNumberRpcRequest>(
                currentTransactionUUID()));
        mBlockNumberRequestSent = true;
        return resultWaitForRpcResponse(RpcMethod::GetBlockNumber);
    }

    // Timeout: no RPC response received in time.
    if (!hasRpcResponse()) {
        error() << "GetBlockNumber RPC timed out";
        return resultDone();
    }

    if (mRpcContext.front()->method() != RpcMethod::GetBlockNumber) {
        error() << "Unexpected RPC response in block number stage";
        return resultDone();
    }

    auto response = popRpcResponse<GetBlockNumberRpcResponse>();
    if (response->status() != RpcResponseStatus::Success) {
        error() << "Block number retrieval failed: " << response->errorMessage();
        return resultDone();
    }

    mCurrentBlockNumber = response->blockNumber();
    mBlockNumberRequestSent = false;
    info() << "Current block number received: " << mCurrentBlockNumber;

    mStep = Stages::Cleanup;
    return resultContinuePreviousState();
}

TransactionResult::SharedConst CleanupHistoricalCryptoDataTransaction::runCleanupStage()
{
    info() << "runCleanupStage";

    if (mStorageHandler == nullptr) {
        error() << "StorageHandler is not configured";
        return resultDone();
    }

    vector<AuditRecord::Shared> candidateAudits;
    IOTransaction::Shared ioTransaction = nullptr;
    try {
        ioTransaction = mStorageHandler->beginTransaction();
        candidateAudits = ioTransaction->auditHandler()->auditsLessEqualThanAuditNumber(
            mTrustLineID,
            mThresholdAuditNumber);
        // Release read-only transaction before doing per-audit cleanup.
        ioTransaction.reset();
    } catch (const exception &e) {
        if (ioTransaction != nullptr) {
            ioTransaction->rollback();
        }
        error() << "Failed to load candidate audits for cleanup. Details: " << e.what();
        return resultDone();
    }

    if (candidateAudits.empty()) {
        info() << "No audits eligible for cleanup";
        return resultDone();
    }

    sort(
        candidateAudits.begin(),
        candidateAudits.end(),
        [](const AuditRecord::Shared &left, const AuditRecord::Shared &right) {
            return left->auditNumber() < right->auditNumber();
        });

    for (const auto &audit : candidateAudits) {
        const auto auditNumber = audit->auditNumber();
        if (auditNumber == kZeroAuditNumber) {
            info() << "Skipping zero audit number";
            continue;
        }

        // Process audit sequentially; stop if cleanup cannot continue safely.
        if (!processAuditCleanup(
                mTrustLineID,
                auditNumber,
                mCurrentBlockNumber)) {
            return resultDone();
        }
    }

    info() << "Historical crypto data cleanup completed";
    return resultDone();
}

bool CleanupHistoricalCryptoDataTransaction::processAuditCleanup(
    const TrustLineID trustLineID,
    const AuditNumber auditNumber,
    const BlockNumber currentBlockNumber)
{
    IOTransaction::Shared ioTransaction = nullptr;
    try {
        ioTransaction = mStorageHandler->beginTransaction();

        // Load receipts for this audit number.
        const auto incomingReceipts =
            ioTransaction->incomingPaymentReceiptHandler()->receiptsByAuditNumber(
                trustLineID,
                auditNumber);
        const auto outgoingReceipts =
            ioTransaction->outgoingPaymentReceiptHandler()->receiptsByAuditNumber(
                trustLineID,
                auditNumber);

        // Collect unique transaction UUIDs from receipts.
        set<TransactionUUID, decltype(&compareTransactionUUID)> transactionUUIDs(
            &compareTransactionUUID);
        for (const auto &receipt : incomingReceipts) {
            transactionUUIDs.insert(receipt->transactionUUID());
        }
        for (const auto &receipt : outgoingReceipts) {
            transactionUUIDs.insert(receipt->transactionUUID());
        }

        if (transactionUUIDs.empty()) {
            info() << "Audit " << auditNumber << " has no receipts, removing audit only";
            ioTransaction->auditHandler()->deleteAuditByNumber(
                trustLineID,
                auditNumber);
            return true;
        }

        // Ensure all related transactions are outside observing window.
        for (const auto &transactionUUID : transactionUUIDs) {
            try {
                const auto effectiveClaimingBlockNumber =
                    ioTransaction->paymentTransactionsHandler()->effectiveClaimingBlockNumber(
                        transactionUUID);
                if (effectiveClaimingBlockNumber >= currentBlockNumber) {
                    info() << "Cleanup halted: observing still possible for "
                           << transactionUUID.stringUUID();
                    return false;
                }
            } catch (const NotFoundError &e) {
                warning() << "Payment transaction missing for receipt "
                          << transactionUUID.stringUUID()
                          << ". Details: " << e.what();
            }
        }

        // Delete receipts and audit atomically.
        ioTransaction->incomingPaymentReceiptHandler()->deleteRecordsByAuditNumber(
            trustLineID,
            auditNumber);
        ioTransaction->outgoingPaymentReceiptHandler()->deleteRecordsByAuditNumber(
            trustLineID,
            auditNumber);
        ioTransaction->auditHandler()->deleteAuditByNumber(
            trustLineID,
            auditNumber);

        // Remove payment transactions that are not referenced anywhere.
        for (const auto &transactionUUID : transactionUUIDs) {
            if (!isTransactionReferenced(ioTransaction, transactionUUID)) {
                ioTransaction->paymentParticipantsVotesHandler()->deleteRecords(
                    transactionUUID);
                ioTransaction->paymentTransactionsHandler()->deleteRecord(
                    transactionUUID);
            }
        }

        info() << "Audit " << auditNumber << " cleanup completed";
        return true;
    } catch (const exception &e) {
        if (ioTransaction != nullptr) {
            ioTransaction->rollback();
        }
        warning() << "Cleanup failed for audit " << auditNumber
                  << ". Details: " << e.what();
        return false;
    }
}

bool CleanupHistoricalCryptoDataTransaction::isTransactionReferenced(
    IOTransaction::Shared ioTransaction,
    const TransactionUUID &transactionUUID) const
{
    return ioTransaction->incomingPaymentReceiptHandler()->isContainsTransaction(transactionUUID) ||
           ioTransaction->outgoingPaymentReceiptHandler()->isContainsTransaction(transactionUUID);
}

const string CleanupHistoricalCryptoDataTransaction::logHeader() const
{
    stringstream s;
    s << "[CleanupHistoricalCryptoDataTA: "
      << currentTransactionUUID().stringUUID() << "] ";
    return s.str();
}
