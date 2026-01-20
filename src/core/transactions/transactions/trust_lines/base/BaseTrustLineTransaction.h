#ifndef VTCPD_BASETRUSTLINETRANSACTION_H
#define VTCPD_BASETRUSTLINETRANSACTION_H

#include "../../base/BaseTransaction.h"
#include "../../../../contractors/ContractorsManager.h"
#include "../../../../trust_lines/manager/TrustLinesManager.h"
#include "../../../../features/FeaturesManager.h"
#include "../../../../crypto/keychain.h"
#include "../../../../crypto/sphincskeys.h"

#include "../../../../subsystems_controller/TrustLinesInfluenceController.h"

#include "../../../../network/messages/trust_lines/AuditMessage.h"
#include "../../../../network/messages/trust_lines/AuditResponseMessage.h"
#include "../../../../network/messages/general/PingMessage.h"

#include "../ConflictResolverInitiatorTransaction.h"

#include <utility>
#include <vector>

class BaseTrustLineTransaction : public BaseTransaction
{

public:
    typedef shared_ptr<BaseTrustLineTransaction> Shared;
    BaseTrustLineTransaction(
        const TransactionType type,
        const SerializedEquivalent equivalent,
        ContractorID contractorID,
        ContractorsManager *contractorsManager,
        TrustLinesManager *trustLines,
        StorageHandler *storageHandler,
        Keystore *keystore,
        FeaturesManager *featuresManager,
        TrustLinesInfluenceController *trustLinesInfluenceController,
        Logger &log);

    BaseTrustLineTransaction(
        const TransactionType type,
        const TransactionUUID &transactionUUID,
        const SerializedEquivalent equivalent,
        ContractorID contractorID,
        ContractorsManager *contractorsManager,
        TrustLinesManager *trustLines,
        StorageHandler *storageHandler,
        Keystore *keystore,
        FeaturesManager *featuresManager,
        TrustLinesInfluenceController *trustLinesInfluenceController,
        Logger &log);

    ContractorID contractorID() const;

protected:
    enum Stages
    {
        Initialization = 1,
        NextAttempt = 2,
        ResponseProcessing = 3,
        Pending = 4,
        ContractorPending = 5,
        NextAttemptPending = 6,
    };

protected:
    TransactionResult::SharedConst sendAuditErrorConfirmation(
        ConfirmationMessage::OperationState errorState);

    pair<BytesShared, size_t> getOwnSerializedAuditData();

    pair<BytesShared, size_t> getContractorSerializedAuditData();

    // Loads finalized receipts and builds the canonical transaction UUID list.
    bool loadReceiptsAndBuildTransactionList();

    // Recalculates receipt totals and audit balance for current list.
    void recalculateReceiptAmounts();

    // Excludes transactions from current list and prepares retry audit data.
    void excludeTransactions(
        const vector<TransactionUUID> &transactionUUIDsToExclude);

    // Serializes own audit data including transaction list hash.
    pair<BytesShared, size_t> getOwnSerializedAuditDataWithTransactionHash() const;

    // Serializes contractor audit data including transaction list hash.
    pair<BytesShared, size_t> getContractorSerializedAuditDataWithTransactionHash() const;

    // Checks if UUID is present in a sorted list.
    bool containsTransactionUUID(
        const vector<TransactionUUID> &transactionUUIDs,
        const TransactionUUID &transactionUUID) const;

protected:
    static const uint32_t kWaitMillisecondsForResponse = 20000;
    static const uint16_t kMaxCountSendingAttempts = 3;
    static const uint32_t kPendingPeriodInMilliseconds = 5000;
    static const uint16_t kMaxPendingAttempts = 5;

protected:
    ContractorsManager *mContractorsManager;
    TrustLinesManager *mTrustLines;
    StorageHandler *mStorageHandler;
    Keystore *mKeysStore;
    FeaturesManager *mFeaturesManager;

    ContractorID mContractorID;
    AuditNumber mAuditNumber;
    crypto::sphincs::Signature::Shared mOwnSignature;

    // Stores all incoming receipts with zero audit number.
    vector<pair<TransactionUUID, TrustLineAmount>> mIncomingReceipts;
    // Stores all outgoing receipts with zero audit number.
    vector<pair<TransactionUUID, TrustLineAmount>> mOutgoingReceipts;
    // Stores the original transaction list for validation.
    vector<TransactionUUID> mOriginalTransactionList;
    // Stores the current transaction list for audit attempts.
    vector<TransactionUUID> mCurrentTransactionList;
    // Sum of incoming receipts excluded from current audit.
    TrustLineAmount mExcludedIncomingReceiptsAmount;
    // Sum of outgoing receipts excluded from current audit.
    TrustLineAmount mExcludedOutgoingReceiptsAmount;
    // Balance used in audit signatures for current list.
    TrustLineBalance mAuditBalance;

    TrustLinesInfluenceController *mTrustLinesInfluenceController;
};


#endif //VTCPD_BASETRUSTLINETRANSACTION_H
