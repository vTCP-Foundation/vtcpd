#ifndef BASEEXCHANGEPAYMENTTRANSACTION_H
#define BASEEXCHANGEPAYMENTTRANSACTION_H

#include "../../../base/BaseTransaction.h"

#include "../../../../../contractors/ContractorsManager.h"
#include "../../../../../equivalents/EquivalentsSubsystemsRouter.h"
#include "../../../../../trust_lines/manager/TrustLinesManager.h"
#include "../../../../../io/storage/interfaces/StorageHandler.h"
#include "../../../../../topology/cache/TopologyCacheManager.h"
#include "../../../../../topology/cache/MaxFlowCacheManager.h"
#include "../../../../../resources/manager/ResourcesManager.h"
#include "../../../../../paths/ExchangePathsManager.h"
#include "../../../../../observing/ObservingHandler.h"
#include "../../../../../resources/resources/BlockNumberRecourse.h"
#include "../../../../../common/exceptions/RuntimeError.h"

#include "../../../../../network/messages/payments/ReceiverInitPaymentRequestMessage.h"
#include "../../../../../network/messages/payments/ReceiverInitPaymentResponseMessage.h"
#include "../../../../../network/messages/payments/CoordinatorReservationRequestMessage.h"
#include "../../../../../network/messages/payments/CoordinatorReservationResponseMessage.h"
#include "../../../../../network/messages/payments/IntermediateNodeReservationRequestMessage.h"
#include "../../../../../network/messages/payments/IntermediateNodeReservationResponseMessage.h"
#include "../../../../../network/messages/payments/CoordinatorCycleReservationRequestMessage.h"
#include "../../../../../network/messages/payments/CoordinatorCycleReservationResponseMessage.h"
#include "../../../../../network/messages/payments/IntermediateNodeCycleReservationRequestMessage.h"
#include "../../../../../network/messages/payments/IntermediateNodeCycleReservationResponseMessage.h"
#include "../../../../../network/messages/payments/ParticipantsVotesMessage.h"
#include "../../../../../network/messages/payments/VotesStatusRequestMessage.hpp"
#include "../../../../../network/messages/payments/FinalPathConfigurationMessage.h"
#include "../../../../../network/messages/payments/FinalPathExchangeConfigurationMessage.h"
#include "../../../../../network/messages/payments/FinalPathCycleConfigurationMessage.h"
#include "../../../../../network/messages/payments/TTLProlongationRequestMessage.h"
#include "../../../../../network/messages/payments/TTLProlongationResponseMessage.h"
#include "../../../../../network/messages/payments/FinalAmountsConfigurationMessage.h"
#include "../../../../../network/messages/payments/FinalAmountsConfigurationResponseMessage.h"
#include "../../../../../network/messages/payments/TransactionPublicKeyHashMessage.h"
#include "../../../../../network/messages/payments/ParticipantsPublicKeysMessage.h"
#include "../../../../../network/messages/payments/ParticipantVoteMessage.h"
#include "../../../../../observing/messages/ObservingClaimAppendRequestMessage.h"

#include "../../../../../network/rpc/requests/GetBlockNumberRpcRequest.h"
#include "../../../../../network/rpc/responses/GetBlockNumberRpcResponse.h"

#include "../../../../../subsystems_controller/SubsystemsController.h"

#include <unordered_set>

using namespace crypto;
namespace signals = boost::signals2;

class BaseExchangePaymentTransaction:
    public BaseTransaction
{

public:
    typedef shared_ptr<BaseExchangePaymentTransaction> Shared;

public:
    typedef signals::signal<void(set<ContractorID>&, const SerializedEquivalent)> BuildCycleThreeNodesSignal;
    typedef signals::signal<void(set<ContractorID>&, const SerializedEquivalent)> BuildCycleFourNodesSignal;

public:
    BaseExchangePaymentTransaction(
        const TransactionType type,
        const SerializedEquivalent equivalent,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        StorageHandler *storageHandler,
        ResourcesManager *resourcesManager,
        ExchangePathsManager *exchangePathsManager,
        ObservingHandler *observingHandler,
        Keystore *keystore,
        Logger &log,
        SubsystemsController *subsystemsController);

    BaseExchangePaymentTransaction(
        const TransactionType type,
        const TransactionUUID &transactionUUID,
        const SerializedEquivalent equivalent,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        StorageHandler *storageHandler,
        ResourcesManager *resourcesManager,
        ExchangePathsManager *exchangePathsManager,
        ObservingHandler *observingHandler,
        Keystore *keystore,
        Logger &log,
        SubsystemsController *subsystemsController);

    BaseExchangePaymentTransaction(
        BytesShared buffer,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        StorageHandler *storageHandler,
        ResourcesManager *resourcesManager,
        ExchangePathsManager *exchangePathsManager,
        ObservingHandler *observingHandler,
        Keystore *keystore,
        Logger &log,
        SubsystemsController *subsystemsController);

    virtual pair<BytesShared, size_t> serializeToBytes() const;

public:
    virtual BaseAddress::Shared coordinatorAddress() const;
    virtual const SerializedPathLengthSize cycleLength() const;
    bool isVotingStage() const;
    void setTransactionState(BaseExchangePaymentTransaction::SerializedStep transactionStage);
    void setObservingParticipantsSignatures(map<PaymentNodeID, sphincs::Signature::Shared> participantsSignatures);

protected:
    // Helper methods for accessing equivalent-specific managers
    TrustLinesManager* trustLinesManager(const SerializedEquivalent equivalent) const;
    TopologyCacheManager* topologyCacheManager(const SerializedEquivalent equivalent) const;
    MaxFlowCacheManager* maxFlowCacheManager(const SerializedEquivalent equivalent) const;
    bool iAmGateway(const SerializedEquivalent equivalent) const;

public:
    enum Stages
    {
        Coordinator_Initialization = 1,
        Coordinator_ReceiverResourceProcessing,
        Coordinator_ReceiverRequestProcessing,
        Coordinator_ReceiverResponseProcessing,
        Coordinator_AmountReservation,
        Coordinator_ShortPathAmountReservationResponseProcessing,
        Coordinator_PreviousNeighborRequestProcessing,
        Coordinator_FinalAmountsConfigurationConfirmation,

        Receiver_CoordinatorRequestApproving,
        Receiver_AmountReservationsProcessing,

        IntermediateNode_PreviousNeighborRequestProcessing,
        IntermediateNode_CoordinatorRequestProcessing,
        IntermediateNode_NextNeighborResponseProcessing,
        IntermediateNode_ReservationProlongation,

        Cycles_WaitForOutgoingAmountReleasing,
        Cycles_WaitForIncomingAmountReleasing,

        Common_ObservingBlockNumberProcessing,
        Common_Voting,
        Common_VotesChecking,
        Common_FinalPathConfigurationChecking,
        Common_Recovery,
        Common_Observing,
        Common_ObservingReject,

        Common_RollbackByOtherTransaction,
        Common_Uncertain,

        // Submitting claim to the observer.
        Observing_AcceptClaim,
        // Polling claim status from the observer.
        Observing_GetClaimStatus
    };

    enum VotesRecoveryStages
    {
        Common_PrepareNodesListToCheckVotes,
        Common_CheckCoordinatorVotesStage,
        Common_CheckIntermediateNodeVotesStage
    };

protected:
    TransactionResult::SharedConst runVotesCheckingStage();
    virtual TransactionResult::SharedConst runVotesConsistencyCheckingStage();
    TransactionResult::SharedConst processParticipantsVotesMessage();
    virtual TransactionResult::SharedConst approve();
    virtual TransactionResult::SharedConst recover(const char* message);
    virtual TransactionResult::SharedConst reject(const char* message);
    TransactionResult::SharedConst runVotesRecoveryParentStage();
    TransactionResult::SharedConst sendVotesRequestMessageAndWaitForResponse(Contractor::Shared contractor);
    TransactionResult::SharedConst processNextNodeToCheckVotes();
    TransactionResult::SharedConst runPrepareListNodesToCheckNodes();
    TransactionResult::SharedConst runCheckCoordinatorVotesStage();
    TransactionResult::SharedConst runCheckIntermediateNodeVotesStage();
    TransactionResult::SharedConst runObservingResultStage();
    TransactionResult::SharedConst runObservingRejectTransaction();

protected:
    const bool reserveOutgoingAmount(
        ContractorID neighborNode,
        const TrustLineAmount& amount,
        const PathID &pathID,
        const SerializedEquivalent equivalent);

    const bool reserveIncomingAmount(
        ContractorID neighborNode,
        const TrustLineAmount& amount,
        const PathID &pathID,
        const SerializedEquivalent equivalent);

    const bool copyReservationFromGlobalReservations(
        ContractorID neighborNode,
        const TrustLineAmount& amount,
        AmountReservation::ReservationDirection reservationDirection,
        const PathID &pathID,
        const SerializedEquivalent equivalent);

    const bool shortageReservation(
        ContractorID kContractor,
        const AmountReservation::ConstShared kReservation,
        const TrustLineAmount &kNewAmount,
        const PathID &pathID,
        const SerializedEquivalent equivalent);

    void saveVotes(IOTransaction::Shared ioTransaction);
    void commit(IOTransaction::Shared ioTransaction);
    void invalidateCachedPathsDueToCommit();
    void rollBack();
    void rollBack(const PathID &pathID);
    void removeAllDataFromStorageConcerningTransaction(IOTransaction::Shared ioTransaction = nullptr);
    uint32_t maxNetworkDelay(const uint16_t totalHopsCount) const;
    const bool contextIsValid(Message::MessageType messageType, bool showErrorMessage = true) const;
    const bool resourceIsValid(BaseResource::ResourceType resourceType) const;
    const bool rpcRequestIsValid(RpcMethod rpcMethod) const;
    void dropNodeReservationsOnPath(PathID pathID);
    void runThreeNodesCyclesTransactions();
    void runFourNodesCyclesTransactions();

    bool updateReservations(const vector<PathReservation> &finalAmounts);
    PathID updateReservation(
        ContractorID contractorID,
        pair<PathID, AmountReservation::ConstShared> &pathIDAndReservation,
        const vector<PathReservation> &finalAmounts);

    size_t reservationsSizeInBytes() const;
    const TrustLineAmount totalReservedAmount(
        AmountReservation::ReservationDirection reservationDirection,
        const SerializedEquivalent equivalent) const;

    virtual void savePaymentOperationIntoHistory(IOTransaction::Shared ioTransaction) = 0;
    virtual bool checkReservationsDirections() const = 0;

    pair<BytesShared, size_t> getSerializedReceipt(
        ContractorID source,
        ContractorID target,
        const TrustLineAmount &amount,
        bool isSource,
        const SerializedEquivalent equivalent);

    bool checkAllNeighborsWithReservationsAreInFinalParticipantsList();
    bool checkAllPublicKeyHashesProperly();
    const TrustLineAmount totalReservedIncomingAmountToNode(
        ContractorID contractorID,
        const SerializedEquivalent equivalent) const;
    bool checkPublicKeysAppropriate();
    pair<BytesShared, size_t> getSerializedParticipantsVotesData(Contractor::Shared contractor);
    bool checkSignaturesAppropriate();
    bool checkMaxClaimingBlockNumber(BlockNumber maxClaimingBlockNumberOnOwnSide);

protected:
    static const uint16_t kMaxMessageTransferLagMSec = 1500;
    static const uint16_t kMaxResourceTransferLagMSec = 2000;
    static const auto kMaxPathLength = 7;
    static const uint32_t kWaitMillisecondsToTryRecoverAgain = 30000;
    static const uint32_t kWaitMillisecondsToTryInitialRecoverAgain = 900000;
    // Delay between observing polls.
    static const uint32_t kObservingCheckPeriodMilliseconds = 60000;
    static const uint8_t kMaxRecoveryAttempts = 30;
    static const uint8_t kMaxSuspendingAttemptsOnFinalAmountsConfirmationStage = 3;
    const PaymentNodeID kCoordinatorPaymentNodeID = 0;
    static const uint64_t kCountBlocksForClaiming = 10000;
    static const uint64_t kAllowableBlockNumberDifference = 10;
    static const uint16_t kDisputeGracePeriodBlocksCount = 4000; 
    static const uint32_t kMaxHoursTransactionDuration = 0;
    static const uint32_t kMaxMinutesTransactionDuration = 0;
    static const uint32_t kMaxSecondsTransactionDuration = 30;

    static Duration& kMaxTransactionDuration()
    {
        static auto duration = Duration(
                                   kMaxHoursTransactionDuration,
                                   kMaxMinutesTransactionDuration,
                                   kMaxSecondsTransactionDuration);
        return duration;
    }

public:
    mutable BuildCycleThreeNodesSignal mBuildCycleThreeNodesSignal;
    mutable BuildCycleFourNodesSignal mBuildCycleFourNodesSignal;

protected:
    ContractorsManager *mContractorsManager;
    EquivalentsSubsystemsRouter *mEquivalentsSubsystemsRouter;
    StorageHandler *mStorageHandler;
    ResourcesManager *mResourcesManager;
    Keystore *mKeysStore;
    SubsystemsController *mSubsystemsController;
    ExchangePathsManager *mExchangePathsManager;
    ObservingHandler *mObservingHandler;

    bool mTTLRequestWasSend;
    bool mTransactionIsVoted;
    ParticipantsVotesMessage::Shared mParticipantsVotesMessage;
    map<ContractorID, vector<pair<PathID, AmountReservation::ConstShared>>> mReservations;
    set<ContractorID> mContractorsForCycles;
    uint8_t mVotesRecoveryStep;
    vector<Contractor::Shared> mNodesToCheckVotes;
    Contractor::Shared mCurrentNodeToCheckVotes;
    uint8_t mCountRecoveryAttempts;
    TrustLineAmount mCommittedAmount;
    map<PaymentNodeID, Contractor::Shared> mPaymentParticipants;
    map<string, PaymentNodeID> mPaymentNodesIds;
    map<string, pair<PaymentNodeID, sphincs::KeyHash::Shared>> mParticipantsPublicKeysHashes;
    map<PaymentNodeID, sphincs::PublicKey::Shared> mParticipantsPublicKeys;
    map<PaymentNodeID, sphincs::Signature::Shared> mParticipantsSignatures;
    sphincs::Signature::Shared mSignedTransaction;
    bool mAllNodesSentConfirmationOnFinalAmountsConfiguration;
    bool mAllNeighborsSentFinalReservations;
    bool mIsSuspendedOnFinalAmountsConfirmationStage;
    uint8_t mCntSuspendingOnFinalAmountsConfirmationStage;
    sphincs::PublicKey::Shared mPublicKey;
    BlockNumber mMaximalClaimingBlockNumber;
    uint16_t mDisputeGracePeriodBlocksCount;
    bool mBlockNumberObtainingInProcess;
    vector<pair<ContractorID, TrustLineAmount>> mOutgoingTransfers;
    vector<pair<ContractorID, TrustLineAmount>> mIncomingTransfers;
    string mPayload;
};

#endif // BASEEXCHANGEPAYMENTTRANSACTION_H
