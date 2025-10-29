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

class ExchangeRatesManager;

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
        ExchangeRatesManager *exchangeRatesManager,
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

    // Helper functions for path filtering and capacity management
    TrustLineAmount calculateTotalReservedAmount();
    TransactionResult::SharedConst proceedToNextStage();
    bool validatePathForProcessing(const OptimalPathResult *pathStats);

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

    /**
     * Handles the detection of condition changes (exchange rate or commission) on a payment path.
     * When an intermediate node reports different conditions than what the coordinator expected,
     * this method validates the change, updates the relevant caches (ExchangeRatesManager for rates,
     * TopologyTrustLinesManager for commissions), drops reservations on the affected path, and marks
     * the path as unusable.
     *
     * @param pathID The identifier of the payment path where condition change was detected
     * @param actualExchangeRate The actual exchange rate reported by the node (mantissa, exponent pair).
     *                          If nullopt, it means the exchange rate is no longer available and should
     *                          be removed from cache.
     * @param actualCommission The actual commission reported by the node. If nullopt, it means the
     *                        commission information was not provided in the response (not changed).
     *                        If set to zero, the commission should be removed from cache.
     * @return Transaction result indicating whether to continue processing other paths or terminate
     */
    TransactionResult::SharedConst handleConditionChange(
        const PathID &pathID,
        const optional<pair<TrustLineAmount, int16_t>> &actualExchangeRate,
        const optional<TrustLineAmount> &actualCommission);

    /**
     * Calculates the expected received amount at the receiver after applying updated conditions
     * (exchange rate and/or commission) at a specific node position in the payment path.
     * This method simulates the flow through the path, applying exchanges and commissions according
     * to the path structure, and uses the updated conditions at the affected node position.
     *
     * @param pathStats Pointer to the optimal path result containing path structure (nodes, equivalents)
     * @param inputFlow The amount that enters the path (at the coordinator/sender)
     * @param affectedNodePosition The position in the path where conditions have changed (0-based index
     *                            in the path.ids vector)
     * @param updatedRate The new exchange rate to use at the affected node position (mantissa, exponent).
     *                   If nullopt, the original rate from path structure is used.
     * @param updatedCommission The new commission to use at the affected node position. If nullopt,
     *                         the original commission from path structure is used.
     * @return The calculated amount that would be received at the final receiver node after applying
     *         all exchanges and commissions, including the updated conditions
     */
    TrustLineAmount calculateReceivedAmountWithUpdatedConditions(
        OptimalPathResult *pathStats,
        const TrustLineAmount &inputFlow,
        const SerializedPositionInPath affectedNodePosition,
        const optional<pair<TrustLineAmount, int16_t>> &updatedRate,
        const optional<TrustLineAmount> &updatedCommission);

    /**
     * Calculates the required optimal flow (sender-side input amount) needed to deliver a target
     * received amount at the receiver, taking into account updated conditions (exchange rate and/or
     * commission) at a specific node position in the payment path.
     *
     * This method performs backward simulation from receiver to sender, inverting exchange rates
     * and adding back commissions. It uses updated conditions at the affected node position to
     * compute how much the sender needs to pay to achieve the target delivery amount.
     *
     * The calculation flows backward through the path:
     * 1. Start with desired receiver amount (mMaxPathReceivedAmount)
     * 2. For each node moving backward toward sender:
     *    - If commission charged at this node: add commission to required amount
     *    - If exchange at this node: invert exchange rate to compute required input
     *    - Use updated rate/commission if this is the affected position
     * 3. Return the computed sender-side flow requirement
     *
     * @param pathStats Pointer to the optimal path result containing path structure (nodes, equivalents)
     * @param desiredReceivedAmount The target amount to be received at the receiver node (typically
     *                             mMaxPathReceivedAmount - the maximum receiver-side capacity)
     * @param affectedNodePosition The position in the path where conditions have changed (0-based index
     *                            in the path.ids vector)
     * @param updatedRate The new exchange rate to use at the affected node position (mantissa, exponent).
     *                   If nullopt, the original rate from path structure is used.
     * @param updatedCommission The new commission to use at the affected node position. If nullopt,
     *                         the original commission from path structure is used.
     * @return The calculated optimal flow (sender-side amount) required to deliver the desired amount
     *         at the receiver, accounting for all exchanges, commissions, and updated conditions
     * @throws ValueError if calculation fails (e.g., zero exchange rate, arithmetic overflow, invalid path)
     */
    TrustLineAmount calculateOptimalFlowWithUpdatedConditions(
        OptimalPathResult *pathStats,
        const TrustLineAmount &desiredReceivedAmount,
        const SerializedPositionInPath affectedNodePosition,
        const optional<pair<TrustLineAmount, int16_t>> &updatedRate,
        const optional<TrustLineAmount> &updatedCommission);

protected:
    EventsInterfaceManager *mEventsInterfaceManager;

    // Command on which current transaction was started
    CreditUsageExchangeCommand::Shared mCommand;
    Contractor::Shared mContractor;

    // Reservation stage contains it's own internal steps counter.
    byte_t mReservationsStage;

    ExchangePathsManager *mExchangePathsManager;
    ExchangeRatesManager *mExchangeRatesManager;
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
