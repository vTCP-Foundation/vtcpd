#ifndef VTCPD_COORDINATOREXCHANGEPAYMENTTRANSACTION_H
#define VTCPD_COORDINATOREXCHANGEPAYMENTTRANSACTION_H

#include "base/BaseExchangePaymentTransaction.h"
#include "../../../paths/ExchangePathsManager.h"
#include "../../../interface/events_interface/interface/EventsInterfaceManager.h"
#include "../../../interface/commands_interface/commands/payments/CreditUsageExchangeCommand.h"
#include "../../../paths/lib/OptimalPathResult.h"
#include "../../../resources/resources/ExchangePathsResource.h"
#include "base/PathReservation.h"
#include "../../../common/exceptions/RuntimeError.h"
#include "../../../../common/exceptions/CallChainBreakException.h"

#include <unordered_map>

class CoordinatorExchangePaymentTransaction : public BaseExchangePaymentTransaction
{

public:
    typedef shared_ptr<CoordinatorExchangePaymentTransaction> Shared;
    typedef shared_ptr<const CoordinatorExchangePaymentTransaction> ConstShared;

public:
    CoordinatorExchangePaymentTransaction(
        const CreditUsageExchangeCommand::Shared command,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        StorageHandler *storageHandler,
        ResourcesManager *resourcesManager,
        ExchangePathsManager *exchangePathsManager,
        Keystore *keystore,
        bool isPaymentTransactionsAllowedDueToObserving,
        EventsInterfaceManager *eventsInterfaceManager,
        Logger &log,
        SubsystemsController *subsystemsController);

    TransactionResult::SharedConst run() override;

    const string logHeader() const override;

    const CommandUUID &commandUUID() const;

protected:
    // Stage handlers
    TransactionResult::SharedConst runPaymentInitializationStage();
    TransactionResult::SharedConst runPathsResourceProcessingStage();
    TransactionResult::SharedConst runReceiverRequestProcessingStage();
    TransactionResult::SharedConst runReceiverResponseProcessingStage();
    TransactionResult::SharedConst runAmountReservationStage();
    TransactionResult::SharedConst runDirectAmountReservationResponseProcessingStage();
    TransactionResult::SharedConst runFinalAmountsConfigurationConfirmation();
    TransactionResult::SharedConst runVotesConsistencyCheckingStage() override;
    TransactionResult::SharedConst runTTLTransactionResponse();

    void savePaymentOperationIntoHistory(IOTransaction::Shared ioTransaction) override;
    bool checkReservationsDirections() const override;

    protected:
    // Coordinator must return command result on transaction finishing.
    // Therefore this methods are overridden.
    TransactionResult::SharedConst approve() override;

    TransactionResult::SharedConst reject(
        const char* message) override;

protected:
    // Results handlers
    TransactionResult::SharedConst resultOK();
    TransactionResult::SharedConst resultForbiddenRun();
    TransactionResult::SharedConst resultForbiddenRunDueObserving();
    TransactionResult::SharedConst resultNoPathsError();
    TransactionResult::SharedConst resultProtocolError();
    TransactionResult::SharedConst resultNoResponseError();
    TransactionResult::SharedConst resultInsufficientFundsError();
    TransactionResult::SharedConst resultNoConsensusError();
    TransactionResult::SharedConst resultUnexpectedError();

protected:
    TransactionResult::SharedConst propagateVotesListAndWaitForVotingResult();

    void addPathForFurtherProcessing(
        const OptimalPathResult& pathResult,
        const TrustLineAmount& pathAmount);
    PathID generateNextPathID();

    /**
     * send messages to all transaction participants with their final amount configuration
     */
    TransactionResult::SharedConst sendFinalAmountsConfigurationToAllParticipants();

    // Amount reservation helper methods
    void initAmountsReservationOnNextPath();
    OptimalPathResult* currentAmountReservationPathStats();
    TransactionResult::SharedConst tryReserveAmountDirectlyOnReceiver(
        const PathID pathID,
        OptimalPathResult *pathStats);
    TransactionResult::SharedConst tryReserveNextIntermediateNodeAmount(OptimalPathResult *pathStats);
    TransactionResult::SharedConst askNeighborToReserveAmount(
        BaseAddress::Shared neighbor,
        OptimalPathResult *pathStats);
    TransactionResult::SharedConst askNeighborToApproveFurtherNodeReservation(
        BaseAddress::Shared neighbor,
        OptimalPathResult *pathStats);
    TransactionResult::SharedConst askRemoteNodeToApproveReservation(
        OptimalPathResult *pathStats,
        BaseAddress::Shared remoteNode,
        const SerializedPositionInPath remoteNodePositionInPath,
        BaseAddress::Shared nextAfterRemoteNode);
    TransactionResult::SharedConst processNeighborAmountReservationResponse();
    TransactionResult::SharedConst processNeighborFurtherReservationResponse();
    TransactionResult::SharedConst processRemoteNodeResponse();
    TransactionResult::SharedConst tryProcessNextPath();
    void switchToNextPath();
    void informAllNodesAboutTransactionFinish();

    void shortageReservationsOnPath(
        ContractorID neighborID,
        const PathID &pathID,
        const TrustLineAmount &kNewAmount);
    void dropReservationsOnPath(
        OptimalPathResult *pathStats,
        const PathID &pathID,
        bool isNeighborReserved = false);

    void addFinalConfigurationOnPath(
        const PathID &pathID,
        OptimalPathResult *pathStats);

    void sendFinalPathConfiguration(
        OptimalPathResult *pathStats,
        const PathID &pathID);

protected:
    EventsInterfaceManager *mEventsInterfaceManager;

    // Command on which current transaction was started
    CreditUsageExchangeCommand::Shared mCommand;
    Contractor::Shared mContractor;

    // Reservation stage contains it's own internal steps counter.
    byte_t mReservationsStage;

    ExchangePathsManager *mExchangePathsManager;
    map<PathID, unique_ptr<OptimalPathResult>> mPathsStats;
    vector<SerializedEquivalent> mExchangeEquivalents;
    SerializedEquivalent mExchangeEquivalent;

    TrustLineAmount mAmount;
    TrustLineAmount mExchangeAmount;  // Amount to be paid in sender equivalent (mExchangeEquivalent)
    CommandUUID mCommandUUID;
    ContractorID mContractorID;
    vector<BaseAddress::Shared> mContractorAddresses;
    bool mIsPaymentTransactionsAllowedDueToObserving;

    map<string, vector<PathReservation>> mNodesFinalAmountsConfiguration;

    // indicates that there are TL with keys absent problem
    bool mNeighborsKeysProblem;

    // indicates that there are participants which have TL with keys absent problem
    bool mParticipantsKeysProblem;

    // counter for participant keys resending attempts
    uint16_t mCountParticipantKeysResending;

    // Current path being processed for amount reservation
    PathID mCurrentAmountReservingPathIdentifier;

    // List of path IDs to process
    vector<PathID> mPathIDs;

    // Current path participants (contractors involved in current path)
    vector<Contractor::Shared> mCurrentPathParticipants;

    // Flags for path processing state
    bool mDirectPathIsAlreadyProcessed;
    bool mIsAuditPendingPathsOccurred;

    // List of rejected trust lines (sender, receiver) pairs
    vector<pair<BaseAddress::Shared, BaseAddress::Shared>> mRejectedTrustLines;

    // List of inaccessible nodes
    vector<BaseAddress::Shared> mInaccessibleNodes;

    uint8_t mCountPathsRecollecting;

    PaymentNodeID mCurrentFreePaymentID;

    // Counter for receiver inaccessibility
    uint8_t mCountReceiverInaccessible;

    // Flag to indicate if the transaction is waiting for exchange paths resource
    bool mIsWaitingForExchangePathsResource;

    static const SerializedPositionInPath kFirstIntermediateNodeIndex = 0;
    static const uint16_t kMaxCountParticipantKeysResending = 5;
    static const uint8_t kMaxReceiverInaccessible = 5;
    static const uint8_t kMaxCountPathsRecollecting = 3;
    static const uint32_t kPathsRecollectingIntervalInMilliseconds = 1000;
    static const uint32_t kAuditRetryingIntervalInMilliseconds = 5000;

    // Maximum age (in seconds) for cached paths to be considered fresh for payment execution
    static constexpr uint32_t kExchangePathsCacheTTLSeconds = 150;
};

#endif //VTCPD_COORDINATOREXCHANGEPAYMENTTRANSACTION_H
