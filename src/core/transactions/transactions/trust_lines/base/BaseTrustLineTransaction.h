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

    pair<BytesShared, size_t> getOwnSerializedAuditData(
        crypto::sphincs::KeyHash::Shared ownPublicKeysHash,
        crypto::sphincs::KeyHash::Shared contractorPublicKeysHash);

    pair<BytesShared, size_t> getContractorSerializedAuditData(
        crypto::sphincs::KeyHash::Shared ownPublicKeysHash,
        crypto::sphincs::KeyHash::Shared contractorPublicKeysHash);

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

    TrustLinesInfluenceController *mTrustLinesInfluenceController;
};


#endif //VTCPD_BASETRUSTLINETRANSACTION_H
