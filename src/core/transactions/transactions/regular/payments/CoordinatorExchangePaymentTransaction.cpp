#include "CoordinatorExchangePaymentTransaction.h"
#include "../../../network/messages/payments/FinalPathExchangeConfigurationMessage.h"
#include "../../../rates/Commission.h"
#include "../../../rates/ExchangeRate.h"
#include "../../../rates/manager/ExchangeRatesManager.h"

#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>
#include <exception>

#include "../../../../topology/manager/TopologyTrustLinesManager.h"
#include "../../../../topology/manager/TopologyTrustLineWithPtr.h"

#include "../../../../common/exceptions/ValueError.h"

namespace {
using CommissionKey = std::pair<ContractorID, SerializedEquivalent>;

TrustLineAmount calculateRequiredInputForPath(
    const OptimalPathResult &pathResult,
    const TrustLineAmount &desiredOutputAmount)
{
    if (desiredOutputAmount == TrustLineAmount(0)) {
        return TrustLineAmount(0);
    }

    const auto &path = pathResult.path();
    if (path.ids.empty() || path.ids.size() != path.equivalents.size()) {
        throw ValueError(
            "CoordinatorExchangePaymentTransaction::calculateRequiredInputForPath: "
            "invalid path structure");
    }

    TrustLineAmount requiredAmount = desiredOutputAmount;

    for (size_t idx = path.ids.size() - 1; idx > 0; --idx) {
        const ContractorID previousNode = path.ids[idx - 1];
        const ContractorID currentNode = path.ids[idx];
        const SerializedEquivalent previousEquivalent = path.equivalents[idx - 1];
        const SerializedEquivalent currentEquivalent = path.equivalents[idx];

        if (previousNode == currentNode && previousEquivalent != currentEquivalent) {
            const auto *exchangeStep = pathResult.findExchangeStep(
                currentNode,
                previousEquivalent,
                currentEquivalent);
            if (!exchangeStep) {
                throw ValueError(
                    "CoordinatorExchangePaymentTransaction::calculateRequiredInputForPath: "
                    "exchange step not found");
            }

            requiredAmount = exchangeStep->invertExchangeForRequiredInput(requiredAmount);
            continue;
        }

        if (idx < path.ids.size() - 1) {
            const auto *commissionStep = pathResult.findExchangeStep(
                currentNode,
                currentEquivalent,
                currentEquivalent);
            if (commissionStep && commissionStep->commission > TrustLineAmount(0)) {
                try {
                    requiredAmount = requiredAmount + commissionStep->commission;
                } catch (const std::exception &e) {
                    throw ValueError(
                        "CoordinatorExchangePaymentTransaction::calculateRequiredInputForPath: "
                        "commission addition overflow: " + std::string(e.what()));
                }
            }
        }
    }

    return requiredAmount;
}

void adjustCommissionsForEstimation(
    OptimalPathResult &pathResult,
    const std::set<CommissionKey> &alreadyConsumed,
    std::vector<CommissionKey> &pendingForPath)
{
    pendingForPath.clear();

    auto &exchangeSteps = pathResult.path().exchangeSteps;
    for (auto &step : exchangeSteps) {
        if (step.fromEquivalent != step.toEquivalent) {
            continue;
        }

        if (step.commission == TrustLineAmount(0)) {
            continue;
        }

        CommissionKey key{step.nodeID, step.fromEquivalent};
        if (alreadyConsumed.find(key) != alreadyConsumed.end()) {
            step.commission = TrustLineAmount(0);
        } else {
            pendingForPath.push_back(key);
        }
    }
}
}

CoordinatorExchangePaymentTransaction::CoordinatorExchangePaymentTransaction(
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
    SubsystemsController *subsystemsController) :

    BaseExchangePaymentTransaction(
        BaseTransaction::CoordinatorPaymentTransaction,
        command->equivalent(),
        contractorsManager,
        equivalentsSubsystemsRouter,
        storageHandler,
        resourcesManager,
        keystore,
        log,
        subsystemsController),
    mExchangePathsManager(exchangePathsManager),
    mExchangeRatesManager(exchangeRatesManager),
    mCommand(command),
    mAmount(command->amount()),
    mCommandUUID(command->UUID()),
    mContractorAddresses(command->contractorAddresses()),
    mExchangeEquivalents(command->exchangeEquivalents()),
    mEventsInterfaceManager(eventsInterfaceManager),
    mReservationsStage(0),
    mIsPaymentTransactionsAllowedDueToObserving(isPaymentTransactionsAllowedDueToObserving),
    mCountParticipantKeysResending(0),
    mDirectPathIsAlreadyProcessed(false),
    mPreviousInaccessibleNodesCount(0),
    mPreviousRejectedTrustLinesCount(0),
    mRebuildingAttemptsCount(0),
    mIsAuditPendingPathsOccurred(false),
    mCountReceiverInaccessible(0),
    mIsWaitingForExchangePathsResource(false),
    mParticipantsKeysProblem(false),
    mNeighborsKeysProblem(false),
    mTestShortCircuitAfterCapacityValidation(false)
{
    mStep = Stages::Coordinator_Initialization;
    mContractor = make_shared<Contractor>(command->contractorAddresses());
    mExchangeEquivalent = command->exchangeEquivalents()[0];
    // Get ContractorID from first address
    if (!mContractorAddresses.empty()) {
        try {
            mContractorID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(mContractorAddresses[0]);
        } catch (...) {
            // Will be set later when contractor is resolved
        }
    }
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::run()
{
    while (true) {
        debug() << "run: stage: " << mStep;
        try {
            switch (mStep) {
            case Stages::Coordinator_Initialization:
                return runPaymentInitializationStage();

            case Stages::Coordinator_ReceiverResourceProcessing:
                return runPathsResourceProcessingStage();

            case Stages::Coordinator_ReceiverRequestProcessing:
                return runReceiverRequestProcessingStage();

            case Stages::Coordinator_ReceiverResponseProcessing:
                return runReceiverResponseProcessingStage();

            case Stages::Coordinator_AmountReservation:
                return runAmountReservationStage();

            case Stages::Coordinator_ShortPathAmountReservationResponseProcessing:
                return runDirectAmountReservationResponseProcessingStage();

            case Stages::Common_ObservingBlockNumberProcessing:
                return sendFinalAmountsConfigurationToAllParticipants();

            case Stages::Coordinator_FinalAmountsConfigurationConfirmation:
                return runFinalAmountsConfigurationConfirmation();

            case Stages::Common_VotesChecking:
                return runVotesConsistencyCheckingStage();

            default:
                throw RuntimeError(
                    "CoordinatorPaymentTransaction::run(): "
                    "invalid transaction step.");
            }
        } catch (CallChainBreakException &e) {
            warning() << e.what();
            // on this case we break call functions chain and prevent stack overflow
            mReservationsStage = 2;
            continue;
        } catch (Exception &e) {
            warning() << e.what();
            auto ioTransaction = mStorageHandler->beginTransaction();
            if (ioTransaction->historyStorage()->whetherOperationWasConducted(currentTransactionUUID())) {
                warning() << "Something happens wrong in method run(), but transaction was conducted";
                return resultOK();
            }
            removeAllDataFromStorageConcerningTransaction(ioTransaction);
            ioTransaction->paymentTransactionsHandler()->deleteRecord(
                mTransactionUUID);
            return reject("Something happens wrong in method run(). Transaction will be rejected");
        }
    }
}

const CommandUUID &CoordinatorExchangePaymentTransaction::commandUUID() const
{
    return mCommandUUID;
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runPaymentInitializationStage()
{
    if (!mIsPaymentTransactionsAllowedDueToObserving) {
        warning() << "It is forbid to run payment transactions due to observing";
        return resultForbiddenRunDueObserving();
    }
    if (!mSubsystemsController->isRunPaymentTransactions()) {
        debug() << "It is forbidden run payment transactions";
        return resultForbiddenRun();
    }
    debug() << "Operation initialised to the node " << mContractor->mainAddress()->fullAddress();
    debug() << "CommandUUID: " << mCommand->UUID();
    debug() << "Operation amount: " << mCommand->amount();
    debug() << "Exchange equivalent: " << mExchangeEquivalent;

    if (mContractor == mContractorsManager->selfContractor()) {
        warning() << "Attempt to initialise operation against itself was prevented. Canceled.";
        return resultProtocolError();
    }

    info() << "Starting payment initialization for exchange payment";

    // Step 1: Check path availability for all exchange equivalents
    vector<SerializedEquivalent> missingEquivalents;
    missingEquivalents.reserve(mExchangeEquivalents.size());

    for (const auto& exchangeEquiv : mExchangeEquivalents) {
        PathCacheKey key{mContractorID, exchangeEquiv, mEquivalent};

        // Check if paths exist and are fresh using custom TTL (150s)
        // retrievePaths() returns nullopt if paths missing OR expired (age >= 150s)
        auto cachedPaths = mExchangePathsManager->retrievePaths(
            key,
            kExchangePathsCacheTTLSeconds);

        if (!cachedPaths) {
            // Paths not found or expired - need collection
            debug() << "Exchange paths missing or expired for equivalent " << exchangeEquiv
                    << " -> " << mEquivalent;
            missingEquivalents.push_back(exchangeEquiv);
        } else {
            debug() << "Found " << cachedPaths->size() << " cached paths for equivalent "
                    << exchangeEquiv << " -> " << mEquivalent;
        }
    }

    // Step 2: If any equivalents missing, request path collection
    if (!missingEquivalents.empty()) {
        info() << "Exchange paths missing or expired for " << missingEquivalents.size()
               << " of " << mExchangeEquivalents.size() << " equivalents, requesting collection";

        mResourcesManager->requestExchangePaths(
            currentTransactionUUID(),
            mContractorAddresses[0], // Main contractor address
            missingEquivalents,
            mEquivalent); // Receiver equivalent

        mStep = Stages::Coordinator_ReceiverResourceProcessing;
        mIsWaitingForExchangePathsResource = true;

        // Wait for ExchangePathsResource
        return resultWaitForResourceTypes(
            {BaseResource::ExchangePaths},
            maxNetworkDelay(4)); // 4 hops for topology collection
    }

    info() << "All exchange paths available in cache, proceeding to path processing";

    // Step 3: All paths available, proceed to path processing stage
    mStep = Stages::Coordinator_ReceiverResourceProcessing;
    return runPathsResourceProcessingStage();
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runPathsResourceProcessingStage()
{
    debug() << "runPathsResourceProcessingStage";

    if (mIsWaitingForExchangePathsResource) {
        if (!resourceIsValid(BaseResource::ExchangePaths)) {
            return resultNoPathsError();
        }
        popNextResource<ExchangePathsResource>();
    }

    // Calculate mExchangeAmount (amount to pay in sender equivalent)
    // using cached paths and calculateFlows() for accurate commission handling
    try {
        PathCacheKey key{mContractorID, mExchangeEquivalent, mEquivalent};
        auto cachedPaths = mExchangePathsManager->retrievePaths(key);

        if (!cachedPaths) {
            warning() << "No cached optimal paths for contractor " << mContractorID
                      << " with sender_eq=" << mExchangeEquivalent
                      << " and receiver_eq=" << mEquivalent;
            return resultNoPathsError();
        }

        // Calculate required payment amount for each path using integer-safe simulation
        TrustLineAmount remainingReceive = mCommand->amount();  // receiver amount
        TrustLineAmount totalPayment = TrustLineAmount(0);
        std::set<CommissionKey> estimationConsumed;

        for (auto pathResult : *cachedPaths) {  // Copy to allow modification
            if (remainingReceive == TrustLineAmount(0)) {
                break;
            }

            TrustLineAmount deliveredAmount = min(remainingReceive, pathResult.received_amount);
            if (deliveredAmount == TrustLineAmount(0)) {
                continue;
            }

            vector<CommissionKey> pathPending;
            adjustCommissionsForEstimation(
                pathResult,
                estimationConsumed,
                pathPending);

            TrustLineAmount pathFlow;
            try {
                pathFlow = calculateRequiredInputForPath(pathResult, deliveredAmount);
            } catch (const ValueError &e) {
                warning() << "Unable to compute required flow for path: " << e.what();
                continue;
            } catch (const std::exception &e) {
                warning() << "Unexpected error while computing required flow: " << e.what();
                continue;
            }

            if (pathFlow > pathResult.optimal_flow) {
                warning() << "Required flow " << pathFlow
                          << " exceeds path capacity " << pathResult.optimal_flow
                          << "; skipping path";
                continue;
            }

            TrustLineAmount requiredPayment = pathFlow;
            try {
                pathResult.calculateFlows(pathFlow);

                if (!pathResult.flows.empty()) {
                    requiredPayment = pathResult.flows.front().first;

                    const TrustLineAmount deliveredByPath = pathResult.flows.back().first;
                    if (deliveredByPath < deliveredAmount) {
                        warning() << "Path delivers only " << deliveredByPath
                                  << " but " << deliveredAmount
                                  << " requested; skipping";
                        continue;
                    }
                    deliveredAmount = deliveredByPath;
                }
            } catch (const std::exception &e) {
                warning() << "calculateFlows failed for path: " << e.what()
                          << "; falling back to direct flow";
            }

            try {
                totalPayment = totalPayment + requiredPayment;
            } catch (const std::exception &e) {
                throw ValueError(
                    "CoordinatorExchangePaymentTransaction::runPaymentInitializationStage: "
                    "failed to accumulate payment: " + std::string(e.what()));
            }

            remainingReceive = remainingReceive - deliveredAmount;

            for (const auto &key : pathPending) {
                estimationConsumed.insert(key);
            }
        }

        if (remainingReceive > TrustLineAmount(0)) {
            warning() << "Insufficient paths to deliver " << mCommand->amount()
                      << " to contractor " << mContractorID
                      << "; can deliver only " << (mCommand->amount() - remainingReceive);
            return resultInsufficientFundsError();
        }

        mExchangeAmount = totalPayment;
        info() << "Calculated exchange amount: " << mExchangeAmount
               << " (sender eq=" << mExchangeEquivalent << ") "
               << "to deliver " << mCommand->amount()
               << " (receiver eq=" << mEquivalent << ")";

    } catch (const exception &e) {
        error() << "Error calculating exchange amount: " << e.what();
        return resultProtocolError();
    }

    // Step 0.4: Check maxAllowablePaymentAmount
    if (exceedsAllowablePaymentAmount(mExchangeAmount)) {
        warning() << "Calculated exchange amount " << mExchangeAmount
                  << " exceeds maximum allowable payment amount "
                  << mCommand->maxAllowablePaymentAmount();
        return resultAllowablePaymentAmountExceeded();
    }

    info() << "Exchange amount " << mExchangeAmount
           << " within allowable limit " << mCommand->maxAllowablePaymentAmount();

    // Check if total outgoing possibilities of this node are not smaller,
    // than total exchange amount (amount to be paid in sender equivalent).
    // In case if so - there is no reason to begin the operation:
    // current node would not be able to pay such an amount.
    const auto kTotalOutgoingPossibilities = *(trustLinesManager(mExchangeEquivalent)->totalOutgoingAmount());
    if (kTotalOutgoingPossibilities < mExchangeAmount) {
        const auto kTotalOutgoingAuditPendingAmount = *(trustLinesManager(mExchangeEquivalent)->totalPossibleOutgoingAmountConsiderToAuditPendingTLs());
        info() << "totalPossibleOutgoingAmountConsiderToAuditPendingTLs " << kTotalOutgoingAuditPendingAmount;
        if (kTotalOutgoingPossibilities + kTotalOutgoingAuditPendingAmount >= mExchangeAmount) {
            info() << "Total outgoing possibilities (" << kTotalOutgoingPossibilities << ") less then operation amount, "
                   << "but there are total outgoing audit pending possibilities (" << kTotalOutgoingAuditPendingAmount
                   << "). Try to collect paths later.";
            mCountPathsRecollecting++;
            if (mCountPathsRecollecting > kMaxCountPathsRecollecting) {
                warning() << "Count rebuilding attempts reaches maximal number. Canceling.";
                return resultInsufficientFundsError();
            }
            return resultAwakeAfterMilliseconds(kAuditRetryingIntervalInMilliseconds);
        }
        warning() << "Total outgoing possibilities (" << kTotalOutgoingPossibilities << ") less then operation amount";
        return resultInsufficientFundsError();
    }

    // Step 1: Iterate through each sender equivalent and add ALL paths without truncation
    // Capacity truncation will be performed later during path processing
    for (const auto& senderEquiv : mExchangeEquivalents) {
        // Step 2: Create cache key for this sender-receiver equivalent combination
        PathCacheKey key{
            mContractorID,
            senderEquiv,
            mEquivalent  // receiver equivalent
        };

        // Step 3: Retrieve optimal paths from ExchangePathsManager
        auto optimalPaths = mExchangePathsManager->retrievePaths(key);

        if (!optimalPaths) {
            // No paths available for this equivalent combination
            debug() << "No cached paths for sender equiv " << senderEquiv;
            continue;
        }

        // Step 4: Add ALL paths with their full capacity (no truncation, no early break)
        // This provides maximum flexibility when paths fail during reservation
        for (const auto& pathResult : *optimalPaths) {
            // Add path with full optimal_flow capacity (sender equivalent)
            // Note: calculateFlows expects input amount in sender equivalent, not receiver
            addPathForFurtherProcessing(pathResult, pathResult.optimal_flow);
        }
    }

    // Step 5: Validate that we have at least some paths available
    if (mPathsStats.empty()) {
        warning() << "No paths available for processing";
        return transactionResultFromCommand(
            mCommand->responseInsufficientFunds());
    }

    info() << "Added " << mPathsStats.size() << " paths for further processing";


    mStep = Stages::Coordinator_ReceiverRequestProcessing;
    return runReceiverRequestProcessingStage();
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runReceiverRequestProcessingStage()
{
    // Sending message to the receiver note to approve the payment receiving.
    sendMessage<ReceiverInitPaymentRequestMessage>(
        mContractor->mainAddress(),
        mEquivalent,
        mContractorsManager->ownAddresses(),
        currentTransactionUUID(),
        mCommand->amount(),
        mCommand->payload());

    mStep = Stages::Coordinator_ReceiverResponseProcessing;
    // delay 4 = 6sec for message delivery guarantee
    return resultWaitForMessageTypes( {
        Message::Payments_ReceiverInitPaymentResponse,
        Message::General_NoEquivalent},
    maxNetworkDelay(4));
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runReceiverResponseProcessingStage()
{
    if (contextIsValid(Message::General_NoEquivalent, false)) {
        warning() << "Receiver hasn't TLs on requested equivalent. Canceling.";
        return resultProtocolError();
    }
    if (!contextIsValid(Message::Payments_ReceiverInitPaymentResponse)) {
        warning() << "Receiver reservation response wasn't received. Canceling.";
        return resultNoResponseError();
    }

    const auto kMessage = popNextMessage<ReceiverInitPaymentResponseMessage>();

    // For exchange transactions, we don't have Audit Pending logic in the same way
    // as regular payments, so we simplify this

    if (kMessage->state() != ReceiverInitPaymentResponseMessage::Accepted) {
        info() << "Receiver rejected payment operation. Canceling.";
        return resultInsufficientFundsError();
    }

    debug() << "Receiver accepted operation. Begin reserving amounts.";
    mCurrentFreePaymentID = kCoordinatorPaymentNodeID;
    auto selfContractor = make_shared<Contractor>(mContractorsManager->ownAddresses());
    mPaymentParticipants.insert(
        make_pair(
            mCurrentFreePaymentID,
            selfContractor));
    mPaymentNodesIds.insert(
        make_pair(
            selfContractor->mainAddress()->fullAddress(),
            mCurrentFreePaymentID));
    mCurrentFreePaymentID++;
    mPaymentParticipants.insert(
        make_pair(
            mCurrentFreePaymentID,
            mContractor));
    mPaymentNodesIds.insert(
        make_pair(
            mContractor->mainAddress()->fullAddress(),
            mCurrentFreePaymentID));
    mCurrentFreePaymentID++;
    mStep = Stages::Coordinator_AmountReservation;
    return runAmountReservationStage();
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runAmountReservationStage()
{
    debug() << "runAmountReservationStage";
    switch (mReservationsStage) {
    case 0: {
        initAmountsReservationOnNextPath();
        mReservationsStage += 1;

        // Note:
        // next section must be executed immediately.
        // (no "break" is needed).
        [[fallthrough]];
    }

    case 1: {
        // nodes can clarify if transaction is still alive
        if (contextIsValid(Message::MessageType::Payments_TTLProlongationRequest, false)) {
            return runTTLTransactionResponse();
        }
        const auto kPathStats = currentAmountReservationPathStats();
        if (!kPathStats->containsIntermediateNodes()) {
            // In case if path doesn't contains intermediate nodes -
            // middleware nodes reservation must be omitted.
            return tryReserveAmountDirectlyOnReceiver(
                       mCurrentAmountReservingPathIdentifier,
                       kPathStats);
        }

        else if (kPathStats->isReadyToSendNextReservationRequest())
            return tryReserveNextIntermediateNodeAmount(kPathStats);

        else if (kPathStats->isWaitingForNeighborReservationResponse())
            return processNeighborAmountReservationResponse();

        else if (kPathStats->isWaitingForNeighborReservationPropagationResponse())
            return processNeighborFurtherReservationResponse();

        else if (kPathStats->isWaitingForReservationResponse())
            return processRemoteNodeResponse();

        throw RuntimeError(
            "CoordinatorExchangePaymentTransaction::runAmountReservationStage: "
            "unexpected behaviour occurred.");
    }

    case 2:
        mReservationsStage = 1;
        return tryProcessNextPath();

    default:
        throw ValueError(
            "CoordinatorExchangePaymentTransaction::runAmountReservationStage: "
            "unexpected reservations stage occurred.");
    }
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runDirectAmountReservationResponseProcessingStage()
{
    debug() << "runDirectAmountReservationResponseProcessingStage";
    auto pathStats = currentAmountReservationPathStats();
    if (not contextIsValid(Message::Payments_IntermediateNodeReservationResponse)) {
        debug() << "No reservation response was received from the receiver node. "
                << "Amount reservation is impossible. Switching to another path.";

        mCountReceiverInaccessible++;
        if (mCountReceiverInaccessible >= kMaxReceiverInaccessible) {
            reject("Contractor is offline. Rollback.");
            return resultNoResponseError();
        }
        dropReservationsOnPath(
            pathStats,
            mCurrentAmountReservingPathIdentifier);
        mStep = Stages::Coordinator_AmountReservation;
        return tryProcessNextPath();
    }

#ifdef TESTS
    mSubsystemsController->testThrowExceptionOnPreviousNeighborRequestProcessingStage();
    mSubsystemsController->testTerminateProcessOnPreviousNeighborRequestProcessingStage();
#endif

    const auto kMessage = popNextMessage<IntermediateNodeReservationResponseMessage>();
    auto receiverID = mContractorsManager->contractorIDByAddress(kMessage->senderAddresses.at(0));
    if (receiverID == ContractorsManager::kNotFoundContractorID) {
        warning() << "Received message is not from neighbor";
        return resultContinuePreviousState();
    }

    // todo : check if sender is really receiver

    // Get sender equivalent from path
    const auto senderEquivalent = pathStats->currentPathEquivalent();

    if (kMessage->state() == IntermediateNodeReservationResponseMessage::RejectedDueContractorKeysAbsence ||
            kMessage->state() == IntermediateNodeReservationResponseMessage::RejectedDueOwnKeysAbsence) {
        warning() << "Receiver node doesn't approved reservation request due to contractor keys absence. "
                  << "Switching to another path.";
        dropReservationsOnPath(
            pathStats,
            mCurrentAmountReservingPathIdentifier);
        mRejectedTrustLines.emplace_back(
            mContractorsManager->ownAddresses().at(0),
            kMessage->senderAddresses.at(0));
        mNeighborsKeysProblem = true;
        if (kMessage->state() == IntermediateNodeReservationResponseMessage::RejectedDueContractorKeysAbsence) {
            publicKeysSharingSignal(receiverID, senderEquivalent);
        }
        mStep = Stages::Coordinator_AmountReservation;
        return tryProcessNextPath();
    }

    if (kMessage->state() == IntermediateNodeReservationResponseMessage::RejectedDueAuditPending) {
        warning() << "Receiver node doesn't approved reservation request due to audit pending. "
                  << "Switching to another path.";
        dropReservationsOnPath(
            pathStats,
            mCurrentAmountReservingPathIdentifier);
        mRejectedTrustLines.emplace_back(
            mContractorsManager->ownAddresses().at(0),
            kMessage->senderAddresses.at(0));
        mIsAuditPendingPathsOccurred = true;
        mStep = Stages::Coordinator_AmountReservation;
        return tryProcessNextPath();
    }

    if (kMessage->state() != IntermediateNodeReservationResponseMessage::Accepted) {
        warning() << "Receiver node rejected reservation. "
                  << "Switching to another path.";
        dropReservationsOnPath(
            pathStats,
            mCurrentAmountReservingPathIdentifier);
        mRejectedTrustLines.emplace_back(
            mContractorsManager->ownAddresses().at(0),
            kMessage->senderAddresses.at(0));
        mStep = Stages::Coordinator_AmountReservation;
        return tryProcessNextPath();
    }

    if (kMessage->amountReserved() != pathStats->optimal_flow) {
        shortageReservationsOnPath(
            receiverID,
            mCurrentAmountReservingPathIdentifier,
            kMessage->amountReserved());
    }

    // Check total reserved amount using sender equivalent
    const auto kTotalAmount = totalReservedAmount(
                                  AmountReservation::Outgoing, senderEquivalent);
    debug() << "Current path reservation finished";
    debug() << "Total collected amount by all paths: " << kTotalAmount;

    if (kTotalAmount > mExchangeAmount) {
        debug() << "Total exchange amount: " << mExchangeAmount;
        return reject("Total collected amount is greater than exchange amount. "
                      "It indicates that some of the nodes doesn't follows the protocol, "
                      "or that an error is present in protocol itself.");
    }

    try {
        addFinalConfigurationOnPath(
            mCurrentAmountReservingPathIdentifier,
            pathStats);
    } catch (const ValueError& e) {
        error() << "Failed to add final configuration: " << e.what();
        return reject("Internal payment error: flow calculation mismatch");
    }

    if (kTotalAmount == mExchangeAmount) {
        debug() << "Total exchange amount: " << mExchangeAmount << ". Collected.";
        debug() << "Begin processing participants votes.";

        mStep = Common_ObservingBlockNumberProcessing;
        mResourcesManager->requestObservingBlockNumber(
            mTransactionUUID);
        return resultWaitForResourceTypes(
        {BaseResource::ObservingBlockNumber},
        maxNetworkDelay(1));
    }
    mStep = Stages::Coordinator_AmountReservation;
    return tryProcessNextPath();
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runFinalAmountsConfigurationConfirmation()
{
    debug() << "runFinalAmountsConfigurationConfirmation";
    if (contextIsValid(Message::Payments_TTLProlongationRequest, false)) {
        return runTTLTransactionResponse();
    }

    if (!contextIsValid(Message::Payments_FinalAmountsConfigurationResponse, false)) {
        removeAllDataFromStorageConcerningTransaction();
        return reject("Some nodes didn't confirm final amount configuration. Transaction rejected.");
    }

    auto kMessage = popNextMessage<FinalAmountsConfigurationResponseMessage>();
    auto senderAddress = kMessage->senderAddresses.at(0);
    debug() << "sender: " << senderAddress->fullAddress();
    if (mPaymentNodesIds.find(senderAddress->fullAddress()) == mPaymentNodesIds.end()) {
        warning() << "Sender is not participant of this transaction";
        return resultContinuePreviousState();
    }
    if (kMessage->state() == FinalAmountsConfigurationResponseMessage::Rejected) {
        removeAllDataFromStorageConcerningTransaction();
        return reject("Haven't reach consensus on reservation. Transaction rejected.");
    }
    debug() << "Sender confirmed final amounts";
    mParticipantsPublicKeys[mPaymentNodesIds[senderAddress->fullAddress()]] = kMessage->publicKey();
    if (mParticipantsPublicKeys.size() < mPaymentNodesIds.size()) {
        debug() << "Some nodes are still not confirmed final amounts. Waiting.";
        return resultWaitForMessageTypes( {
            Message::Payments_FinalAmountsConfigurationResponse,
            Message::Payments_TTLProlongationRequest},
        maxNetworkDelay(2));
    }

    debug() << "All nodes confirmed final configuration. Begin processing participants votes.";
    return propagateVotesListAndWaitForVotingResult();
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::sendFinalAmountsConfigurationToAllParticipants()
{
    debug() << "sendFinalAmountsConfigurationToAllParticipants";

    // suspending process means that we already have block number resource
    if (!mIsSuspendedOnFinalAmountsConfirmationStage) {
        if (!resourceIsValid(BaseResource::ObservingBlockNumber)) {
            return resultUnexpectedError();
        }
        auto blockNumberResource = popNextResource<BlockNumberRecourse>();
        mMaximalClaimingBlockNumber = blockNumberResource->actualObservingBlockNumber() + kCountBlocksForClaiming;
    }

    // check if reservation to contractor present
    auto receiverID = mContractorsManager->contractorIDByAddress(mContractor->mainAddress());
    const auto contractorNodeReservations = mReservations.find(receiverID);
    if (contractorNodeReservations != mReservations.end()) {
        if (contractorNodeReservations->second.size() > 1) {
            return reject("Coordinator has more than one reservation to contractor");
        }
    }

    // Check for KeysSharing state in reservations
    for (auto const &reservation : mReservations) {
        // Get equivalent from first reservation to access correct TrustLinesManager
        if (!reservation.second.empty()) {
            const auto equivalent = reservation.second.front().second->equivalent();
            auto trustLines = trustLinesManager(equivalent);

            if (trustLines->trustLineState(reservation.first) == TrustLine::KeysSharing) {
                info() << "reservation with " << reservation.first << " in KeysSharing state";
                mIsSuspendedOnFinalAmountsConfirmationStage = true;
                if (mCntSuspendingOnFinalAmountsConfirmationStage < kMaxSuspendingAttemptsOnFinalAmountsConfirmationStage) {
                    mCntSuspendingOnFinalAmountsConfirmationStage++;
                    info() << "suspend " << mCntSuspendingOnFinalAmountsConfirmationStage << " time";
                    return resultAwakeAfterMilliseconds(maxNetworkDelay(2));
                }
                info() << "Suspending done max times. Continue";
                break;
            }
        }
    }

#ifdef TESTS
    mSubsystemsController->testForbidSendMessageOnFinalAmountClarificationStage();
#endif

    mParticipantsPublicKeys.clear();
    auto ioTransaction = mStorageHandler->beginTransaction();
    // Ensure reusable payment key exists and load it
    mKeysStore->ensurePaymentKeyExists(ioTransaction);
    mPublicKey = ioTransaction->paymentKeysHandler()->getOwnPublicKey();
    mParticipantsPublicKeys.insert(
        make_pair(
            kCoordinatorPaymentNodeID,
            mPublicKey));

    for (auto const &paymentNodeIdAndContractor : mPaymentParticipants) {
        if (paymentNodeIdAndContractor.first == kCoordinatorPaymentNodeID) {
            continue;
        }
        auto participantID = mContractorsManager->contractorIDByAddress(paymentNodeIdAndContractor.second->mainAddress());

        // if coordinator has reservations with current node it also send receipt
        if (mReservations.find(participantID) != mReservations.end()) {
            // Group outgoing reservations by equivalent
            map<SerializedEquivalent, TrustLineAmount> amountsByEquivalent;
            for (const auto &pathIDAndReservation : mReservations[participantID]) {
                // Only process outgoing reservations
                if (pathIDAndReservation.second->direction() == AmountReservation::Outgoing) {
                    SerializedEquivalent equiv = pathIDAndReservation.second->equivalent();
                    if (amountsByEquivalent.find(equiv) == amountsByEquivalent.end()) {
                        amountsByEquivalent[equiv] = TrustLine::kZeroAmount();
                    }
                    amountsByEquivalent[equiv] = amountsByEquivalent[equiv] + pathIDAndReservation.second->amount();
                }
            }

            // Create signature for each equivalent
            vector<pair<SerializedEquivalent, sphincs::Signature::Shared>> signatures;
            for (const auto &[equivalent, amount] : amountsByEquivalent) {
                auto trustLines = trustLinesManager(equivalent);
                auto keyChain = mKeysStore->keychain(trustLines->trustLineID(participantID));

                auto serializedOutgoingReceiptData = getSerializedReceipt(
                        mContractorsManager->idOnContractorSide(participantID),
                        participantID,
                        amount,
                        true,
                        equivalent);
                auto signature = keyChain.sign(
                                                 ioTransaction,
                                                 serializedOutgoingReceiptData.first,
                                                 serializedOutgoingReceiptData.second);
                if (!keyChain.saveOutgoingPaymentReceipt(
                            ioTransaction,
                            trustLines->auditNumber(participantID),
                            mTransactionUUID,
                            amount,
                            signature)) {
                    return reject("Can't save outgoing receipt. Rejected.");
                }
                signatures.emplace_back(equivalent, signature);
                debug() << "Created receipt for equivalent " << equivalent << " with amount " << amount;
            }

            info() << "send final amount configuration to " << paymentNodeIdAndContractor.second->mainAddress()->fullAddress()
                   << " with " << signatures.size() << " receipt(s)";

            // Get final amounts configuration for this node
            auto nodeKey = paymentNodeIdAndContractor.second->mainAddress()->fullAddress();
            const auto& nodeConfig = mNodesFinalAmountsConfiguration.find(nodeKey);
            info() << "final amount configuration size: " << nodeConfig->second.size();

            if (nodeConfig != mNodesFinalAmountsConfiguration.end()) {
                // Send with PathReservation vector (new constructor)
                sendMessage<FinalAmountsConfigurationMessage>(
                    paymentNodeIdAndContractor.second->mainAddress(),
                    mEquivalent,
                    mContractorsManager->ownAddresses(),
                    currentTransactionUUID(),
                    nodeConfig->second,  // vector<PathReservation>
                    mPaymentParticipants,
                    mMaximalClaimingBlockNumber,
                    signatures);
            } else {
                // Node not found in configuration - send empty vector
                warning() << "Node " << nodeKey << " not found in mNodesFinalAmountsConfiguration";
                vector<PathReservation> emptyReservations;
                sendMessage<FinalAmountsConfigurationMessage>(
                    paymentNodeIdAndContractor.second->mainAddress(),
                    mEquivalent,
                    mContractorsManager->ownAddresses(),
                    currentTransactionUUID(),
                    emptyReservations,
                    mPaymentParticipants,
                    mMaximalClaimingBlockNumber,
                    signatures);
            }
        } else {
            info() << "send final amount configuration to " << paymentNodeIdAndContractor.second->mainAddress()->fullAddress();

            // Get final amounts configuration for this node
            auto nodeKey = paymentNodeIdAndContractor.second->mainAddress()->fullAddress();
            const auto& nodeConfig = mNodesFinalAmountsConfiguration.find(nodeKey);
            info() << "final amount configuration size: " << nodeConfig->second.size();

            if (nodeConfig != mNodesFinalAmountsConfiguration.end()) {
                // Send with PathReservation vector (new constructor)
                sendMessage<FinalAmountsConfigurationMessage>(
                    paymentNodeIdAndContractor.second->mainAddress(),
                    mEquivalent,
                    mContractorsManager->ownAddresses(),
                    currentTransactionUUID(),
                    nodeConfig->second,  // vector<PathReservation>
                    mPaymentParticipants,
                    mMaximalClaimingBlockNumber);
            } else {
                // Node not found in configuration - send empty vector
                warning() << "Node " << nodeKey << " not found in mNodesFinalAmountsConfiguration";
                vector<PathReservation> emptyReservations;
                sendMessage<FinalAmountsConfigurationMessage>(
                    paymentNodeIdAndContractor.second->mainAddress(),
                    mEquivalent,
                    mContractorsManager->ownAddresses(),
                    currentTransactionUUID(),
                    emptyReservations,
                    mPaymentParticipants,
                    mMaximalClaimingBlockNumber);
            }
        }
    }

    debug() << "Total count of all participants with coordinator is " << mPaymentParticipants.size();

    mStep = Coordinator_FinalAmountsConfigurationConfirmation;
    return resultWaitForMessageTypes( {
        Message::Payments_FinalAmountsConfigurationResponse,
        Message::Payments_TTLProlongationRequest},
    maxNetworkDelay(6));
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runVotesConsistencyCheckingStage()
{
    debug() << "runVotesConsistencyCheckingStage";
    // Intermediate node or Receiver can send request if transaction is still alive.
    if (contextIsValid(Message::Payments_TTLProlongationRequest, false)) {
        return runTTLTransactionResponse();
    }

#ifdef TESTS
    mSubsystemsController->testThrowExceptionOnVoteConsistencyStage();
    mSubsystemsController->testTerminateProcessOnVoteConsistencyStage();
#endif

    if (! contextIsValid(Message::Payments_ParticipantVote)) {
        warning() << "Coordinator didn't receive all messages with votes";

        if (mCountParticipantKeysResending >= kMaxCountParticipantKeysResending) {
            removeAllDataFromStorageConcerningTransaction();
            return reject("Too many resending attempts");
        }

        info() << "Resend participants public keys " << mCountParticipantKeysResending << " times";
        // resend message with all public keys to participants which don't send participant vote
        for (const auto &paymentNodeIdAndAddress : mPaymentParticipants) {
            if (paymentNodeIdAndAddress.first == kCoordinatorPaymentNodeID) {
                continue;
            }
            if (mParticipantsSignatures.find(paymentNodeIdAndAddress.first) != mParticipantsSignatures.end()) {
                continue;
            }
            info() << "Resend to " << paymentNodeIdAndAddress.second->mainAddress()->fullAddress();
            sendMessage<ParticipantsPublicKeysMessage>(
                paymentNodeIdAndAddress.second->mainAddress(),
                mEquivalent,
                mContractorsManager->ownAddresses(),
                currentTransactionUUID(),
                mParticipantsPublicKeys);
        }
        mCountParticipantKeysResending++;
        return resultWaitForMessageTypes( {
            Message::Payments_ParticipantVote,
            Message::Payments_TTLProlongationRequest},
        maxNetworkDelay(4));
    }

    const auto kMessage = popNextMessage<ParticipantVoteMessage>();
    auto sender = make_shared<Contractor>(kMessage->senderAddresses);
    debug() << "Participant vote message received from " << sender->mainAddress()->fullAddress();
    if (mPaymentNodesIds.find(sender->mainAddress()->fullAddress()) == mPaymentNodesIds.end()) {
        warning() << "Sender is not participant of current transaction";
        return resultContinuePreviousState();
    }
    if (kMessage->state() == ParticipantVoteMessage::Rejected) {
        removeAllDataFromStorageConcerningTransaction();
        return reject("Participant rejected voting. Rolling back");
    }
    auto participantSignature = kMessage->signature();
    auto participantPaymentID = mPaymentNodesIds[sender->mainAddress()->fullAddress()];
    auto participantPublicKey = mParticipantsPublicKeys[participantPaymentID];
    auto participantSerializedVotesData = getSerializedParticipantsVotesData(
            sender);
    // todo if we store participants public keys on database, then we should use KeyChain,
    // or we can check sign directly from mParticipantsPublicKeys
    if (!participantSignature->verify(
                *participantPublicKey,
                participantSerializedVotesData.first.get(),
                participantSerializedVotesData.second)) {
        removeAllDataFromStorageConcerningTransaction();
        return reject("Participant signature is incorrect. Rolling back");
    }
    info() << "Participant signature is correct";
    mParticipantsSignatures.insert(
        make_pair(
            participantPaymentID,
            participantSignature));

    if (mParticipantsSignatures.size() + 1 == mPaymentParticipants.size()) {
        info() << "all participants sign their data";

        auto serializedOwnVotesData = getSerializedParticipantsVotesData(
                                          mContractorsManager->selfContractor());
        {
            auto ioTransaction = mStorageHandler->beginTransaction();
            auto signature = mKeysStore->signPaymentTransaction(
                                 ioTransaction,
                                 serializedOwnVotesData.first,
                                 serializedOwnVotesData.second);

            if (!signature.has_value()) {
                error() << "Can't sign the payment transaction. See logs for the details";
                return resultUnexpectedError();
            }

            mParticipantsSignatures.insert(
                make_pair(
                    kCoordinatorPaymentNodeID,
                    signature.value()));

            ioTransaction->paymentTransactionsHandler()->saveRecord(
                mTransactionUUID,
                mMaximalClaimingBlockNumber);
        }
        debug() << "Voted +";
        const auto ownAddresses = mContractorsManager->ownAddresses();
        mParticipantsVotesMessage = make_shared<ParticipantsVotesMessage>(
                                        mEquivalent,
                                        ownAddresses,
                                        mTransactionUUID,
                                        mParticipantsSignatures);
        return approve();
    }

    info() << "Not all participants send theirs signs";
    return resultWaitForMessageTypes( {
        Message::Payments_ParticipantVote,
        Message::Payments_TTLProlongationRequest},
    maxNetworkDelay(3));
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runTTLTransactionResponse()
{
    debug() << "runTTLTransactionResponse";
    auto kMessage = popNextMessage<TTLProlongationRequestMessage>();
    auto senderAddress = kMessage->senderAddresses.at(0);
    info() << "sender " << senderAddress->fullAddress();
    if (mPaymentParticipants.empty()) {
        // reservation stage
        if (senderAddress == mContractor->mainAddress()) {
            sendMessage<TTLProlongationResponseMessage>(
                senderAddress,
                mEquivalent,
                mContractorsManager->ownAddresses(),
                currentTransactionUUID(),
                TTLProlongationResponseMessage::Continue);
            debug() << "Send clarifying message that transactions is alive";
        } else {
            // Check if sender is in mNodesFinalAmountsConfiguration
            bool foundInConfig = false;
            for (const auto &pathReservation : mNodesFinalAmountsConfiguration) {
                // Note: We need to check if sender is part of any path
                // For simplicity, we'll allow Continue for now
                // TODO: improve this check when we have path node tracking
                foundInConfig = true;
                break;
            }

            if (foundInConfig || !mNodesFinalAmountsConfiguration.empty()) {
                // coordinator has configuration for requested node
                sendMessage<TTLProlongationResponseMessage>(
                    senderAddress,
                    mEquivalent,
                    mContractorsManager->ownAddresses(),
                    currentTransactionUUID(),
                    TTLProlongationResponseMessage::Continue);
                debug() << "Send clarifying message that transactions is alive";
            } else {
                sendMessage<TTLProlongationResponseMessage>(
                    senderAddress,
                    mEquivalent,
                    mContractorsManager->ownAddresses(),
                    currentTransactionUUID(),
                    TTLProlongationResponseMessage::Finish);
                debug() << "Send transaction finishing message";
            }
        }
    } else {
        // voting stage
        if (mPaymentNodesIds.find(senderAddress->fullAddress()) != mPaymentNodesIds.end()) {
            sendMessage<TTLProlongationResponseMessage>(
                senderAddress,
                mEquivalent,
                mContractorsManager->ownAddresses(),
                currentTransactionUUID(),
                TTLProlongationResponseMessage::Continue);
            debug() << "Send clarifying message that transactions is alive";
        } else {
            sendMessage<TTLProlongationResponseMessage>(
                senderAddress,
                mEquivalent,
                mContractorsManager->ownAddresses(),
                currentTransactionUUID(),
                TTLProlongationResponseMessage::Finish);
            info() << "Sender is not a member of this transaction. Continue previous state";
            debug() << "Send transaction finishing message";
        }
    }
    return resultContinuePreviousState();
}

void CoordinatorExchangePaymentTransaction::initAmountsReservationOnNextPath()
{
    if (mPathsStats.empty())
        throw NotFoundError(
            "CoordinatorExchangePaymentTransaction::initAmountsReservationOnNextPath: "
            "no paths are available.");

    // Step 1: Check if more capacity needed before taking first path
    TrustLineAmount totalReserved = calculateTotalReservedAmount();
    if (totalReserved >= mAmount) {
        // Sufficient capacity reserved, no more paths needed
        info() << "Path filtering: sufficient capacity already reserved ("
               << totalReserved << " >= " << mAmount << "), proceeding to next stage";

        // Request observing block number resource and transition to next stage
        mResourcesManager->requestObservingBlockNumber(mTransactionUUID);
        mStep = Stages::Common_ObservingBlockNumberProcessing;

        throw CallChainBreakException("Sufficient capacity reserved, proceeding to next stage");
    }

    TrustLineAmount remainingNeeded = mAmount - totalReserved;
    debug() << "Path filtering: remaining needed = " << remainingNeeded
            << " (total reserved: " << totalReserved << ", target: " << mAmount << ")";

    // Step 2: Find valid path through filtering
    while (!mPathIDs.empty()) {
        PathID nextPathID = *mPathIDs.cbegin();
        auto pathStatsIt = mPathsStats.find(nextPathID);

        if (pathStatsIt == mPathsStats.end()) {
            warning() << "Path filtering: next path not found in mPathsStats, pathID=" << nextPathID;
            mPathIDs.erase(mPathIDs.cbegin());
            continue;
        }

        OptimalPathResult *pathStats = pathStatsIt->second.get();

        // Step 2.5: Check if path is valid (may have been invalidated by condition changes)
        if (!pathStats->isValid()) {
            warning() << "Path filtering: path " << nextPathID << " is invalid, skipping";
            releasePathCommissions(nextPathID, true);
            mPathsStats.erase(nextPathID);
            mPathIDs.erase(mPathIDs.cbegin());
            continue;
        }

        // Step 3: Validate path for processing (check inaccessible nodes and rejected trust lines)
        if (!validatePathForProcessing(pathStats)) {
            // Path contains bad nodes/trustlines, mark unusable and try next
            pathStats->setUnusable();
            releasePathCommissions(nextPathID, true);
            mPathsStats.erase(nextPathID);
            mPathIDs.erase(mPathIDs.cbegin());
            continue;
        }

        // Step 3.5: Adjust commissions according to already consumed pairs
        prepareCommissionsForPath(pathStats, nextPathID);

        // Step 4: Check if truncation needed
        if (pathStats->received_amount > remainingNeeded) {
            info() << "Path capacity truncation: available=" << pathStats->received_amount
                   << ", needed=" << remainingNeeded;

            try {
                // Calculate truncated input amount that delivers exactly remainingNeeded
                TrustLineAmount truncatedInput = calculateRequiredInputForPath(
                    *pathStats, remainingNeeded);

                // Update path capacity
                pathStats->received_amount = remainingNeeded;
                pathStats->optimal_flow = truncatedInput;

                // Recalculate flows for reservation
                pathStats->calculateFlows(truncatedInput);

                // Re-apply commission adjustments after recalculation
                prepareCommissionsForPath(pathStats, nextPathID);

                info() << "Path capacity truncated: input=" << truncatedInput
                       << ", output=" << remainingNeeded;

            } catch (const std::exception &e) {
                // Truncation calculation failed, skip this path and try next
                error() << "Path capacity truncation failed: " << e.what()
                        << ", skipping path";
                pathStats->setUnusable();
                releasePathCommissions(nextPathID, true);
                mPathsStats.erase(nextPathID);
                mPathIDs.erase(mPathIDs.cbegin());
                continue;
            }
        }

        // Step 5: Path is valid and truncated if needed, proceed with it
        mCurrentAmountReservingPathIdentifier = nextPathID;
        debug() << "[" << mCurrentAmountReservingPathIdentifier << "] path reservation initialized";
        mCurrentPathParticipants.clear();
        return;
    }

    // No more paths available
    throw NotFoundError(
        "CoordinatorExchangePaymentTransaction::initAmountsReservationOnNextPath: "
        "no paths are available.");
}

OptimalPathResult* CoordinatorExchangePaymentTransaction::currentAmountReservationPathStats()
{
    return mPathsStats[mCurrentAmountReservingPathIdentifier].get();
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::tryReserveAmountDirectlyOnReceiver(
    const PathID pathID,
    OptimalPathResult *pathStats)
{
    debug() << "tryReserveAmountDirectlyOnReceiver";
#ifdef INTERNAL_ARGUMENTS_VALIDATION
    assert(!pathStats->containsIntermediateNodes());
#endif

    if (mDirectPathIsAlreadyProcessed) {
        warning() << "Direct path reservation attempt occurred, but previously it was already processed. "
                  << "It seems that paths collection contains direct path several times. "
                  << "This one and all other similar path would be rejected. "
                  << "Switching to the other path.";

        pathStats->setUnusable();
        return tryProcessNextPath();
    }
    mDirectPathIsAlreadyProcessed = true;

    debug() << "Direct path occurred (coordinator -> receiver). "
            << "Trying to reserve amount directly on the receiver side.";

    auto receiverID = mContractorsManager->contractorIDByAddress(
                          mContractor->mainAddress());
    if (receiverID == ContractorsManager::kNotFoundContractorID) {
        warning() << "Direct path wrong because receiver is not neighbor of contractor";
        pathStats->setUnusable();
        return tryProcessNextPath();
    }

    // For exchange transactions, use sender equivalent from path (first equivalent)
    const auto senderEquivalent = pathStats->currentPathEquivalent();
    auto senderTrustLines = trustLinesManager(senderEquivalent);

    if (!senderTrustLines->trustLineIsActive(receiverID)) {
        warning() << "Invalid TL state " << senderTrustLines->trustLineState(receiverID);
        if (senderTrustLines->trustLineState(receiverID) == TrustLine::AuditPending ||
                senderTrustLines->trustLineState(receiverID) == TrustLine::KeysSharing) {
            mIsAuditPendingPathsOccurred = true;
        }
        pathStats->setUnusable();
        return tryProcessNextPath();
    }

    if (!senderTrustLines->trustLineOwnKeysPresent(receiverID)) {
        warning() << "There are no own keys on TL with receiver. Switching to another path.";
        pathStats->setUnusable();
        mNeighborsKeysProblem = true;
        publicKeysSharingSignal(receiverID, senderEquivalent);
        return tryProcessNextPath();
    }

    // Check if local reservation is possible.
    const auto kAvailableOutgoingAmount = senderTrustLines->outgoingTrustAmountConsideringReservations(receiverID);
    if (*kAvailableOutgoingAmount == TrustLine::kZeroAmount()) {
        debug() << "There is no direct outgoing amount available for the receiver node. "
                << "Switching to another path.";

        pathStats->setUnusable();
        return tryProcessNextPath();
    }

    // Note: try reserve remaining part of exchange amount (in sender equivalent)
    const auto kRemainingAmountForProcessing =
        mExchangeAmount - totalReservedAmount(AmountReservation::Outgoing, senderEquivalent);

    // Reserving amount locally (in sender equivalent).
    const auto kReservationAmount = min(kRemainingAmountForProcessing, *kAvailableOutgoingAmount);
    if (not reserveOutgoingAmount(
                receiverID,
                kReservationAmount,
                pathID,
                senderEquivalent)) {
        warning() << "Can't reserve amount locally. Switching to another path.";

        pathStats->setUnusable();
        return tryProcessNextPath();
    }

    // Reserving on the contractor side
    vector<pair<PathID, ConstSharedTrustLineAmount>> reservations;
    reservations.emplace_back(
        mCurrentAmountReservingPathIdentifier,
        make_shared<const TrustLineAmount>(kReservationAmount));

#ifdef TESTS
    mSubsystemsController->testForbidSendMessageToReceiverOnReservationStage();
#endif

    debug() << "Send reservations size: " << reservations.size();
    sendMessage<IntermediateNodeReservationRequestMessage>(
        receiverID,
        senderEquivalent,
        mContractorsManager->ownAddresses(),
        mTransactionUUID,
        reservations);

    debug() << "Reservation request for " << kReservationAmount << " sent directly to the receiver node.";

    mStep = Stages::Coordinator_ShortPathAmountReservationResponseProcessing;
    return resultWaitForMessageTypes(
    {Message::Payments_IntermediateNodeReservationResponse},
    maxNetworkDelay(2));
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::tryReserveNextIntermediateNodeAmount(
    OptimalPathResult *pathStats)
{
    debug() << "tryReserveNextIntermediateNodeAmount";
    try {
        const auto remoteAddressAndPos = pathStats->nextIntermediateNodeAndPos();
        const auto remoteAddress = remoteAddressAndPos.first;
        const auto remoteNodePositionInPath = remoteAddressAndPos.second;

        if (remoteNodePositionInPath == kFirstIntermediateNodeIndex) {
            if (pathStats->isNeighborAmountReserved())
                return askNeighborToApproveFurtherNodeReservation(
                           remoteAddress,
                           pathStats);

            else
                return askNeighborToReserveAmount(
                           remoteAddress,
                           pathStats);
        } else {
            debug() << "Processing " << int(remoteNodePositionInPath)
                    << " node in path: (" << remoteAddress->fullAddress() << ").";

            const auto& pathNodes = pathStats->path().nodes;
            if (static_cast<size_t>(remoteNodePositionInPath + 1) >= pathNodes.size()) {
                throw ValueError("Invalid path position");
            }
            const auto nextAfterRemoteNodeAddress = pathNodes[remoteNodePositionInPath + 1];
            return askRemoteNodeToApproveReservation(
                       pathStats,
                       remoteAddress,
                       remoteNodePositionInPath,
                       nextAfterRemoteNodeAddress);
        }
    } catch (NotFoundError &) {
        debug() << "No unprocessed paths are left. Requested amount can't be collected. Canceling.";
        rollBack();
        informAllNodesAboutTransactionFinish();
        return resultInsufficientFundsError();
    }
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::askNeighborToReserveAmount(
    BaseAddress::Shared neighbor,
    OptimalPathResult *path)
{
    debug() << "askNeighborToReserveAmount " << neighbor->fullAddress();

    // Get sender equivalent from path (first equivalent)
    const auto senderEquivalent = path->firstPathEquivalent();
    debug() << "Sender equivalent: " << senderEquivalent;
    auto senderTrustLines = trustLinesManager(senderEquivalent);

    auto neighborID = mContractorsManager->contractorIDByAddress(neighbor);
    if (neighborID == ContractorsManager::kNotFoundContractorID) {
        warning() << "Contractor " << neighbor->fullAddress() << " is not a neighbor";
        throw RuntimeError(
            "CoordinatorExchangePaymentTransaction::askNeighborToReserveAmount: "
            "invalid first level node occurred. ");
    }

    if (!senderTrustLines->trustLineIsPresent(neighborID)) {
        warning() << "Invalid path occurred. Node is not listed in first level contractors list.";
        throw RuntimeError(
            "CoordinatorExchangePaymentTransaction::askNeighborToReserveAmount: "
            "invalid first level TL occurred. ");
    }

    if (!senderTrustLines->trustLineIsActive(neighborID)) {
        warning() << "Invalid TL state " << senderTrustLines->trustLineState(neighborID);
        if (senderTrustLines->trustLineState(neighborID) == TrustLine::AuditPending ||
                senderTrustLines->trustLineState(neighborID) == TrustLine::KeysSharing) {
            mIsAuditPendingPathsOccurred = true;
        }
        path->setUnusable();
        throw CallChainBreakException("Break call chain for preventing call loop");
    }

    if (!senderTrustLines->trustLineOwnKeysPresent(neighborID)) {
        warning() << "There are no own keys on TL with neighbor. Switching to another path.";
        path->setUnusable();
        mNeighborsKeysProblem = true;
        publicKeysSharingSignal(neighborID, senderEquivalent);
        // after signal keys will be shared and tx can pass
        mIsAuditPendingPathsOccurred = true;
        throw CallChainBreakException("Break call chain for preventing call loop");
    }

    if (!senderTrustLines->trustLineContractorKeysPresent(neighborID)) {
        warning() << "There are no contractors keys on TL with neighbor. Switching to another path.";
        path->setUnusable();
        mNeighborsKeysProblem = true;
        throw CallChainBreakException("Break call chain for preventing call loop");
    }

    const auto kReservationAmount = path->optimal_flow;

    if (kReservationAmount == 0) {
        debug() << "No payment amount is available. Switching to another path.";
        mRejectedTrustLines.emplace_back(
            mContractorsManager->ownAddresses().at(0),
            neighbor);
        path->setUnusable();
        throw CallChainBreakException("Break call chain for preventing call loop");
    }

    if (not reserveOutgoingAmount(
                neighborID,
                kReservationAmount,
                mCurrentAmountReservingPathIdentifier,
                senderEquivalent)) {
        warning() << "Can't reserve amount locally. Switching to another path.";
        path->setUnusable();
        throw CallChainBreakException("Break call chain for preventing call loop");
    }

    path->setNodeState(
        kFirstIntermediateNodeIndex,
        OptimalPathResult::NeighbourReservationRequestSent);

    vector<PathReservation> reservations;
    reservations.emplace_back(
        mCurrentAmountReservingPathIdentifier,
        make_shared<const TrustLineAmount>(kReservationAmount),
        senderEquivalent,
        PathReservation::Outgoing);

    if (mNodesFinalAmountsConfiguration.find(neighbor->fullAddress()) != mNodesFinalAmountsConfiguration.end()) {
        // add existing neighbor reservations
        const auto kNeighborReservations = mNodesFinalAmountsConfiguration[neighbor->fullAddress()];
        reservations.insert(
            reservations.end(),
            kNeighborReservations.begin(),
            kNeighborReservations.end());
    }
    debug() << "Prepared for sending reservations size: " << reservations.size();

#ifdef TESTS
    mSubsystemsController->testForbidSendRequestToIntNodeOnReservationStage(
        neighbor,
        kReservationAmount);
#endif

    sendMessage<IntermediateNodeReservationRequestMessage>(
        neighborID,
        senderEquivalent,
        mContractorsManager->idOnContractorSide(neighborID),
        mTransactionUUID,
        reservations);

    debug() << "Amount reservation request sent to the neighbor node (" << neighbor->fullAddress() << ") ["
        << kReservationAmount << ", " << senderEquivalent << "])";

    return resultWaitForMessageTypes( {
        Message::Payments_IntermediateNodeReservationResponse,
        Message::Payments_TTLProlongationRequest,
        Message::General_NoEquivalent},
    maxNetworkDelay(2));
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::askNeighborToApproveFurtherNodeReservation(
    BaseAddress::Shared neighbor,
    OptimalPathResult *pathStats)
{
    debug() << "askNeighborToApproveFurtherNodeReservation " << neighbor->fullAddress();

    // Get the next node after neighbor from path
    const auto& pathNodes = pathStats->path().nodes;
    if (pathNodes.size() < 2) {
        throw RuntimeError("askNeighborToApproveFurtherNodeReservation: path too short");
    }
    const auto kNextAfterNeighborNode = pathNodes[kFirstIntermediateNodeIndex + 1];

    // Note:
    // no check of "neighbor" node is needed here.
    // It was done on previous step.
    const auto pathFlow = pathStats->currentPathFlow();

    vector<PathReservation> reservations;
    reservations.emplace_back(
        mCurrentAmountReservingPathIdentifier,
        make_shared<const TrustLineAmount>(pathFlow.first),
        pathFlow.second,
        PathReservation::Outgoing);

    if (mNodesFinalAmountsConfiguration.find(kNextAfterNeighborNode->fullAddress()) !=
        mNodesFinalAmountsConfiguration.end()) {
        // add existing next after neighbor node reservations
        const auto kNeighborReservations = mNodesFinalAmountsConfiguration[kNextAfterNeighborNode->fullAddress()];

        reservations.insert(
            reservations.end(),
            kNeighborReservations.begin(),
            kNeighborReservations.end());
    }
    debug() << "Prepared for sending reservations size: " << reservations.size();

    // Determine expected conditions for neighbor node
    optional<pair<TrustLineAmount, int16_t>> expectedRate;
    optional<TrustLineAmount> expectedCommission;

    const auto &path = pathStats->path();

    // IMPORTANT: path.nodes is deduplicated, but path.ids/path.equivalents can have duplicates for exchanges.
    // Neighbor is the first intermediate node (position 0 in path.nodes).

    // Use flows to determine incoming equivalent to neighbor
    // For neighbor (first intermediate node), previousPathFlow() returns flows[0] which is the incoming flow
    const auto incomingFlow = pathStats->previousPathFlow();
    SerializedEquivalent incomingEquiv = incomingFlow.second;

    debug() << "Neighbor " << neighbor->fullAddress()
            << ", neighborPositionInPath=" << static_cast<int>(kFirstIntermediateNodeIndex)
            << ", incoming equiv from flows=" << incomingEquiv;

    // Build mapping from path.nodes indices to path.ids indices
    // This simulates the deduplication process used in addPathForFurtherProcessing
    vector<SerializedPositionInPath> nodeToIdsMapping;
    ContractorID lastAddedID = 0; // sender

    for (SerializedPositionInPath idx = 1; idx < path.ids.size(); ++idx) {
        ContractorID currentID = path.ids[idx];
        if (currentID == 0) continue; // skip sender if appears again

        // Check if this is a consecutive duplicate (for in-place exchange)
        bool isDuplicate = (currentID == lastAddedID);

        if (!isDuplicate) {
            // This position in path.ids corresponds to a new entry in path.nodes
            nodeToIdsMapping.push_back(idx);
            lastAddedID = currentID;
        }
    }

    debug() << "Built nodeToIdsMapping with " << nodeToIdsMapping.size() << " entries";

    // Map neighbor position (kFirstIntermediateNodeIndex = 0) to position in path.ids
    SerializedPositionInPath positionInIds = 0;
    bool foundPosition = false;

    if (kFirstIntermediateNodeIndex < nodeToIdsMapping.size()) {
        positionInIds = nodeToIdsMapping[kFirstIntermediateNodeIndex];
        foundPosition = true;
        debug() << "Mapped neighborPositionInPath=" << static_cast<int>(kFirstIntermediateNodeIndex)
                << " to positionInIds=" << static_cast<int>(positionInIds)
                << ", ids[" << positionInIds << "]=" << path.ids[positionInIds];
    }

    if (!foundPosition) {
        warning() << "Could not find neighbor position in path.ids";
        // Send request without expected conditions
        sendMessage<CoordinatorReservationRequestMessage>(
            neighbor,
            pathFlow.second,
            mContractorsManager->ownAddresses(),
            mTransactionUUID,
            reservations,
            kNextAfterNeighborNode);

        debug() << "Further amount reservation request sent to the node (" << neighbor->fullAddress() << ") ["
                << pathFlow.first << ", " << pathFlow.second << "]"
                << ", next node - (" << kNextAfterNeighborNode->fullAddress() << ")";

        pathStats->setNodeState(
            kFirstIntermediateNodeIndex,
            OptimalPathResult::ReservationRequestSent);

        return resultWaitForMessageTypes( {
            Message::Payments_CoordinatorReservationResponse,
            Message::Payments_TTLProlongationRequest},
        maxNetworkDelay(4));
    }

    // Get outgoing equivalent at neighbor from path.equivalents
    SerializedEquivalent outgoingEquiv = path.equivalents[positionInIds + 1];
    // Get neighbor's ContractorID from path.ids
    ContractorID neighborID = path.ids[positionInIds];

    debug() << "Neighbor " << neighbor->fullAddress() << " at position "
            << static_cast<int>(positionInIds) << " in path.ids"
            << ", neighborID=" << neighborID
            << ", incoming equiv=" << incomingEquiv
            << ", outgoing equiv=" << outgoingEquiv;

    // Check if neighbor is exchanger (different equivalents)
    if (incomingEquiv != outgoingEquiv) {
        // Neighbor is exchanger - find exchange rate
        const auto *exchangeStep = pathStats->findExchangeStep(
            neighborID,
            incomingEquiv,
            outgoingEquiv);

        if (exchangeStep) {
            expectedRate = make_pair(exchangeStep->exchangeRate, exchangeStep->exchangeRateShift);
            debug() << "Expected exchange rate: " << exchangeStep->exchangeRate
                    << " * 10^" << exchangeStep->exchangeRateShift;
        }
    }
    // Check if neighbor charges commission (same equivalent)
    else {
        const auto *commissionStep = pathStats->findExchangeStep(
            neighborID,
            incomingEquiv,
            incomingEquiv);

        if (commissionStep && commissionStep->commission > TrustLineAmount(0)) {
            expectedCommission = commissionStep->commission;
            debug() << "Expected commission: " << commissionStep->commission;
        }
    }

#ifdef TESTS
    mSubsystemsController->testForbidSendMessageToCoordinatorOnReservationStage(
        neighbor,
        pathFlow.first);
#endif

    // Create request message with expected conditions
    if (expectedRate) {
        sendMessage<CoordinatorReservationRequestMessage>(
            neighbor,
            pathFlow.second,
            mContractorsManager->ownAddresses(),
            mTransactionUUID,
            reservations,
            kNextAfterNeighborNode,
            expectedRate->first,
            expectedRate->second);
    } else if (expectedCommission) {
        sendMessage<CoordinatorReservationRequestMessage>(
            neighbor,
            pathFlow.second,
            mContractorsManager->ownAddresses(),
            mTransactionUUID,
            reservations,
            kNextAfterNeighborNode,
            *expectedCommission);
    } else {
        sendMessage<CoordinatorReservationRequestMessage>(
            neighbor,
            pathFlow.second,
            mContractorsManager->ownAddresses(),
            mTransactionUUID,
            reservations,
            kNextAfterNeighborNode);
    }

    debug() << "Further amount reservation request sent to the node (" << neighbor->fullAddress() << ") ["
            << pathFlow.first << ", " << pathFlow.second << "]" 
            << ", next node - (" << kNextAfterNeighborNode->fullAddress() << ")";

    pathStats->setNodeState(
        kFirstIntermediateNodeIndex,
        OptimalPathResult::ReservationRequestSent);

    // delay is equal 4 because in IntermediateNodePaymentTransaction::runCoordinatorRequestProcessingStage delay is 2
    return resultWaitForMessageTypes( {
        Message::Payments_CoordinatorReservationResponse,
        Message::Payments_TTLProlongationRequest},
    maxNetworkDelay(4));
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::askRemoteNodeToApproveReservation(
    OptimalPathResult *pathStats,
    BaseAddress::Shared remoteNode,
    const SerializedPositionInPath remoteNodePositionInPath,
    BaseAddress::Shared nextAfterRemoteNode)
{
    debug() << "askRemoteNodeToApproveReservation";
    const auto pathFlow = pathStats->currentPathFlow();
    vector<PathReservation> reservations;
    reservations.emplace_back(
        mCurrentAmountReservingPathIdentifier,
        make_shared<const TrustLineAmount>(pathFlow.first),
        pathFlow.second,
        PathReservation::Outgoing);

    if (mNodesFinalAmountsConfiguration.find(nextAfterRemoteNode->fullAddress()) !=
        mNodesFinalAmountsConfiguration.end()) {
        // add existing next after remote node reservations
        const auto kRemoteNodeReservations = mNodesFinalAmountsConfiguration[nextAfterRemoteNode->fullAddress()];
        reservations.insert(
            reservations.end(),
            kRemoteNodeReservations.begin(),
            kRemoteNodeReservations.end());
    }
    debug() << "Prepared for sending reservations size: " << reservations.size();

    // Get sender equivalent from path
    const auto senderEquivalent = pathFlow.second;

    // Determine expected conditions for remote node
    optional<pair<TrustLineAmount, int16_t>> expectedRate;
    optional<TrustLineAmount> expectedCommission;

    const auto &path = pathStats->path();

    // IMPORTANT: remoteNodePositionInPath is an index into path.nodes (deduplicated array),
    // but path.equivalents corresponds to path.ids (which can have duplicates for exchanges).

    // Use flows to determine incoming equivalent to remote node
    // previousPathFlow() returns the INCOMING flow to the current remote node
    const auto incomingFlow = pathStats->previousPathFlow();
    SerializedEquivalent incomingEquiv = incomingFlow.second;

    debug() << "Remote node " << remoteNode->fullAddress()
            << ", remoteNodePositionInPath=" << static_cast<int>(remoteNodePositionInPath)
            << ", incoming equiv from flows=" << incomingEquiv;

    // Build mapping from path.nodes indices to path.ids indices
    // This simulates the deduplication process used in addPathForFurtherProcessing
    vector<SerializedPositionInPath> nodeToIdsMapping;
    ContractorID lastAddedID = 0; // sender

    for (SerializedPositionInPath idx = 1; idx < path.ids.size(); ++idx) {
        ContractorID currentID = path.ids[idx];
        if (currentID == 0) continue; // skip sender if appears again

        // Check if this is a consecutive duplicate (for in-place exchange)
        bool isDuplicate = (currentID == lastAddedID);

        if (!isDuplicate) {
            // This position in path.ids corresponds to a new entry in path.nodes
            nodeToIdsMapping.push_back(idx);
            lastAddedID = currentID;
        }
    }

    debug() << "Built nodeToIdsMapping with " << nodeToIdsMapping.size() << " entries";

    // Map remoteNodePositionInPath to position in path.ids
    SerializedPositionInPath positionInIds = 0;
    bool foundPosition = false;

    if (remoteNodePositionInPath < nodeToIdsMapping.size()) {
        positionInIds = nodeToIdsMapping[remoteNodePositionInPath];
        foundPosition = true;
        debug() << "Mapped remoteNodePositionInPath=" << static_cast<int>(remoteNodePositionInPath)
                << " to positionInIds=" << static_cast<int>(positionInIds)
                << ", ids[" << positionInIds << "]=" << path.ids[positionInIds];
    }

    if (!foundPosition) {
        warning() << "Could not find remote node position in path.ids";
        // Send request without expected conditions
        sendMessage<CoordinatorReservationRequestMessage>(
            remoteNode,
            senderEquivalent,
            mContractorsManager->ownAddresses(),
            mTransactionUUID,
            reservations,
            nextAfterRemoteNode);

        pathStats->setNodeState(
            remoteNodePositionInPath,
            OptimalPathResult::ReservationRequestSent);

        debug() << "Further amount reservation request sent to the node (" << remoteNode->fullAddress() << ") ["
                << pathFlow.first << ", " << pathFlow.second << "]"
                << ", next node - (" << nextAfterRemoteNode->fullAddress() << ")]";

        return resultWaitForMessageTypes( {
            Message::Payments_CoordinatorReservationResponse,
            Message::Payments_TTLProlongationRequest},
        maxNetworkDelay(4));
    }

    // Get outgoing equivalent at remote node from path.equivalents
    SerializedEquivalent outgoingEquiv = path.equivalents[positionInIds + 1];
    // Get remote node's ContractorID from path.ids
    ContractorID remoteNodeID = path.ids[positionInIds];

    debug() << "Remote node " << remoteNode->fullAddress() << " at position "
            << static_cast<int>(positionInIds) << " in path.ids"
            << ", remoteNodeID=" << remoteNodeID
            << ", incoming equiv=" << incomingEquiv
            << ", outgoing equiv=" << outgoingEquiv;

    // Check if node is exchanger (different equivalents at same node ID)
    if (incomingEquiv != outgoingEquiv) {
        // Node is exchanger - find exchange rate
        const auto *exchangeStep = pathStats->findExchangeStep(
            remoteNodeID,
            incomingEquiv,
            outgoingEquiv);

        if (exchangeStep) {
            expectedRate = make_pair(exchangeStep->exchangeRate, exchangeStep->exchangeRateShift);
            debug() << "Expected exchange rate: " << exchangeStep->exchangeRate
                    << " * 10^" << exchangeStep->exchangeRateShift;
        }
    }
    // Check if node charges commission (same equivalent, commission present)
    else {
        const auto *commissionStep = pathStats->findExchangeStep(
            remoteNodeID,
            incomingEquiv,
            incomingEquiv);

        if (commissionStep && commissionStep->commission > TrustLineAmount(0)) {
            expectedCommission = commissionStep->commission;
            debug() << "Expected commission: " << commissionStep->commission;
        }
    }

#ifdef TESTS
    mSubsystemsController->testForbidSendMessageToCoordinatorOnReservationStage(
        remoteNode,
        pathFlow.first);
#endif

    // Create request message with expected conditions
    if (expectedRate) {
        sendMessage<CoordinatorReservationRequestMessage>(
            remoteNode,
            senderEquivalent,
            mContractorsManager->ownAddresses(),
            mTransactionUUID,
            reservations,
            nextAfterRemoteNode,
            expectedRate->first,
            expectedRate->second);
    } else if (expectedCommission) {
        sendMessage<CoordinatorReservationRequestMessage>(
            remoteNode,
            senderEquivalent,
            mContractorsManager->ownAddresses(),
            mTransactionUUID,
            reservations,
            nextAfterRemoteNode,
            *expectedCommission);
    } else {
        sendMessage<CoordinatorReservationRequestMessage>(
            remoteNode,
            senderEquivalent,
            mContractorsManager->ownAddresses(),
            mTransactionUUID,
            reservations,
            nextAfterRemoteNode);
    }

    pathStats->setNodeState(
        remoteNodePositionInPath,
        OptimalPathResult::ReservationRequestSent);

    debug() << "Further amount reservation request sent to the node (" << remoteNode->fullAddress() << ") ["
            << pathFlow.first << ", " << pathFlow.second << "]" 
            << ", next node - (" << nextAfterRemoteNode->fullAddress() << ")]";

    // delay is equal 4 because in IntermediateNodePaymentTransaction::runCoordinatorRequestProcessingStage delay is 2
    return resultWaitForMessageTypes( {
        Message::Payments_CoordinatorReservationResponse,
        Message::Payments_TTLProlongationRequest},
    maxNetworkDelay(4));
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::processNeighborAmountReservationResponse()
{
    debug() << "processNeighborAmountReservationResponse";

    // nodes can clarify if transaction is still alive
    if (contextIsValid(Message::Payments_TTLProlongationRequest, false)) {
        return runTTLTransactionResponse();
    }

    if (contextIsValid(Message::General_NoEquivalent, false)) {
        warning() << "Neighbor hasn't TLs on requested equivalent";
        dropReservationsOnPath(
            currentAmountReservationPathStats(),
            mCurrentAmountReservingPathIdentifier);
        return tryProcessNextPath();
    }

    if (!contextIsValid(Message::Payments_IntermediateNodeReservationResponse)) {
        debug() << "No neighbor node response received. Switching to another path.";
        // dropping reservation to first node
        dropReservationsOnPath(
            currentAmountReservationPathStats(),
            mCurrentAmountReservingPathIdentifier);

        // sending message to receiver that transaction continues
        sendMessage<TTLProlongationResponseMessage>(
            mContractor->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            TTLProlongationResponseMessage::Continue);

        // remote node is inaccessible, we add it to offline nodes
        const auto kPathStats = currentAmountReservationPathStats();
        const auto addressAndPos = kPathStats->currentIntermediateNodeAndPos();
        mInaccessibleNodes.push_back(addressAndPos.first);
        debug() << addressAndPos.first->fullAddress() << " was added to offline nodes";

        return tryProcessNextPath();
    }

    auto message = popNextMessage<IntermediateNodeReservationResponseMessage>();
    auto neighborAddress = message->senderAddresses.at(0);
    info() << "Neighbor " << neighborAddress->fullAddress() << " send response";

    auto neighborID = mContractorsManager->contractorIDByAddress(neighborAddress);
    if (neighborID == ContractorsManager::kNotFoundContractorID) {
        warning() << "Sender is not a neighbor. Continue previous state";
        return resultContinuePreviousState();
    }

    if (message->pathID() != mCurrentAmountReservingPathIdentifier) {
        warning() << "Neighbor send response on wrong path "
                  << message->pathID() << ". Continue previous state";
        return resultContinuePreviousState();
    }

    // Get sender equivalent from current path
    const auto kPathStats = currentAmountReservationPathStats();
    const auto senderEquivalent = kPathStats->currentPathEquivalent();
    auto senderTrustLines = trustLinesManager(senderEquivalent);

    if (message->state() == IntermediateNodeReservationResponseMessage::Closed) {
        warning() << "Neighbor node doesn't approved reservation request";
        return reject("Desynchronization in reservation with Receiver occurred. Transaction closed.");
    }

    if (message->state() == IntermediateNodeReservationResponseMessage::Rejected) {
        warning() << "Neighbor node doesn't approved reservation request";
        dropReservationsOnPath(
            kPathStats,
            mCurrentAmountReservingPathIdentifier);
        mRejectedTrustLines.emplace_back(
            mContractorsManager->ownAddresses().at(0),
            neighborAddress);
        return tryProcessNextPath();
    }

    if (message->state() == IntermediateNodeReservationResponseMessage::RejectedDueContractorKeysAbsence ||
            message->state() == IntermediateNodeReservationResponseMessage::RejectedDueOwnKeysAbsence) {
        warning() << "Neighbor node doesn't approved reservation request due to keys absence";
        dropReservationsOnPath(
            kPathStats,
            mCurrentAmountReservingPathIdentifier);
        mRejectedTrustLines.emplace_back(
            mContractorsManager->ownAddresses().at(0),
            neighborAddress);
        mNeighborsKeysProblem = true;
        if (message->state() == IntermediateNodeReservationResponseMessage::RejectedDueContractorKeysAbsence) {
            info() << "Keys sharing signal";
            publicKeysSharingSignal(neighborID, senderEquivalent);
            senderTrustLines->setIsOwnKeysPresent(neighborID, false);
        } else {
            senderTrustLines->setIsContractorKeysPresent(neighborID, false);
        }
        return tryProcessNextPath();
    }

    if (message->state() == IntermediateNodeReservationResponseMessage::RejectedDueAuditPending) {
        warning() << "Neighbor node doesn't approved reservation request due to audit pending";
        dropReservationsOnPath(
            kPathStats,
            mCurrentAmountReservingPathIdentifier);
        mRejectedTrustLines.emplace_back(
            mContractorsManager->ownAddresses().at(0),
            neighborAddress);
        mIsAuditPendingPathsOccurred = true;
        return tryProcessNextPath();
    }

    if (message->state() != IntermediateNodeReservationResponseMessage::Accepted) {
        return reject("Unexpected message state. Protocol error. Transaction closed.");
    }

    if (message->amountReserved() == 0) {
        warning() << "Neighbor node doesn't approved reservation request regarding to 0 amount";
        dropReservationsOnPath(
            kPathStats,
            mCurrentAmountReservingPathIdentifier);
        mRejectedTrustLines.emplace_back(
            mContractorsManager->ownAddresses().at(0),
            neighborAddress);
        return tryProcessNextPath();
    }

    debug() << "Neighbor approved reservation request.";
    auto path = currentAmountReservationPathStats();
    path->setNodeState(
        kFirstIntermediateNodeIndex, OptimalPathResult::NeighbourReservationApproved);

    if (message->amountReserved() != path->optimal_flow) {
        shortageReservationsOnPath(
            neighborID,
            mCurrentAmountReservingPathIdentifier,
            message->amountReserved());
    }

    return runAmountReservationStage();
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::processNeighborFurtherReservationResponse()
{
    debug() << "processNeighborFurtherReservationResponse";
    if (!contextIsValid(Message::Payments_CoordinatorReservationResponse)) {
        dropReservationsOnPath(
            currentAmountReservationPathStats(),
            mCurrentAmountReservingPathIdentifier,
            true);
        // sending message to receiver that transaction continues
        sendMessage<TTLProlongationResponseMessage>(
            mContractor->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            TTLProlongationResponseMessage::Continue);

        // remote node is inaccessible, we add it to offline nodes
        const auto kPathStats = currentAmountReservationPathStats();
        const auto addressAndPos = kPathStats->currentIntermediateNodeAndPos();
        mInaccessibleNodes.push_back(addressAndPos.first);
        debug() << addressAndPos.first->fullAddress() << " was added to offline nodes";

        debug() << "Switching to another path.";
        return tryProcessNextPath();
    }

    auto message = popNextMessage<CoordinatorReservationResponseMessage>();
    auto neighborAddress = message->senderAddresses.at(0);
    info() << "Neighbor " << neighborAddress->fullAddress() << " sent response";
    // todo: check message sender

    auto neighborID = mContractorsManager->contractorIDByAddress(neighborAddress);
    if (neighborID == ContractorsManager::kNotFoundContractorID) {
        warning() << "Sender is not a neighbor. Continue previous state";
        return resultContinuePreviousState();
    }

    if (message->pathID() != mCurrentAmountReservingPathIdentifier) {
        warning() << "Neighbor send response on wrong path "
                  << message->pathID() << " . Continue previous state";
        return resultContinuePreviousState();
    }

    if (message->state() == CoordinatorReservationResponseMessage::Closed) {
        return reject("Desynchronization in reservation with Receiver occurred. Transaction closed.");
    }

    if (message->state() == CoordinatorReservationResponseMessage::NextNodeInaccessible) {
        warning() << "Next node after neighbor is inaccessible. Rejecting request.";
        dropReservationsOnPath(
            currentAmountReservationPathStats(),
            mCurrentAmountReservingPathIdentifier);

        // next after remote node is inaccessible, we add it to offline nodes
        const auto kPathStats = currentAmountReservationPathStats();
        const auto addressAndPos = kPathStats->currentIntermediateNodeAndPos();
        const auto& pathNodes = kPathStats->path().nodes;

        if (static_cast<size_t>(addressAndPos.second + 1) >= pathNodes.size()) {
            warning() << "Invalid path position for next node";
            return tryProcessNextPath();
        }

        const auto nextNodeAddress = pathNodes[addressAndPos.second + 1];
        if (nextNodeAddress == mContractor->mainAddress()) {
            mCountReceiverInaccessible++;
            if (mCountReceiverInaccessible >= kMaxReceiverInaccessible) {
                reject("Contractor is offline. Rollback.");
                return resultNoResponseError();
            }
        } else {
            mInaccessibleNodes.push_back(nextNodeAddress);
            debug() << nextNodeAddress->fullAddress() << " was added to offline nodes";
        }

        // sending message to receiver that transaction continues
        sendMessage<TTLProlongationResponseMessage>(
            mContractor->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            TTLProlongationResponseMessage::Continue);

        return tryProcessNextPath();
    }

    if (message->state() == CoordinatorReservationResponseMessage::Rejected) {
        warning() << "Neighbor node doesn't accepted coordinator request.";
        dropReservationsOnPath(
            currentAmountReservationPathStats(),
            mCurrentAmountReservingPathIdentifier);
        // processed trustLine was rejected, we add it to Rejected TrustLines
        const auto kPathStats = currentAmountReservationPathStats();
        const auto neighborAddressAndPos = kPathStats->currentIntermediateNodeAndPos();
        const auto& pathNodes = kPathStats->path().nodes;

        if (static_cast<size_t>(neighborAddressAndPos.second + 1) >= pathNodes.size()) {
            warning() << "Invalid path position for next neighbor";
            return tryProcessNextPath();
        }

        const auto nextNeighborAddress = pathNodes[neighborAddressAndPos.second + 1];
        mRejectedTrustLines.emplace_back(
            neighborAddressAndPos.first,
            nextNeighborAddress);
        // sending message to receiver that transaction continues
        sendMessage<TTLProlongationResponseMessage>(
            mContractor->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            TTLProlongationResponseMessage::Continue);
        return tryProcessNextPath();
    }

    if (message->state() == CoordinatorReservationResponseMessage::RejectedDueOwnKeysAbsence or
            message->state() == CoordinatorReservationResponseMessage::RejectedDueContractorKeysAbsence) {
        warning() << "Neighbor node doesn't accepted coordinator request due to keys absence";
        dropReservationsOnPath(
            currentAmountReservationPathStats(),
            mCurrentAmountReservingPathIdentifier);
        mRejectedTrustLines.emplace_back(
            mContractorsManager->ownAddresses().at(0),
            neighborAddress);
        mParticipantsKeysProblem = true;
        // sending message to receiver that transaction continues
        sendMessage<TTLProlongationResponseMessage>(
            mContractor->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            TTLProlongationResponseMessage::Continue);
        return tryProcessNextPath();
    }

    if (message->state() == CoordinatorReservationResponseMessage::RejectedDueAuditPending) {
        warning() << "Neighbor node doesn't accepted coordinator request due to audit pending";
        dropReservationsOnPath(
            currentAmountReservationPathStats(),
            mCurrentAmountReservingPathIdentifier);
        mRejectedTrustLines.emplace_back(
            mContractorsManager->ownAddresses().at(0),
            neighborAddress);
        mIsAuditPendingPathsOccurred = true;
        // sending message to receiver that transaction continues
        sendMessage<TTLProlongationResponseMessage>(
            mContractor->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            TTLProlongationResponseMessage::Continue);
        return tryProcessNextPath();
    }

    // Handle condition change rejection (exchange rate or commission mismatch)
    if (message->state() == CoordinatorReservationResponseMessage::RejectedDueConditionsChanged) {
        info() << "Condition change detected from neighbor on pathID=" << mCurrentAmountReservingPathIdentifier;

        // Extract actual conditions from response
        auto actualExchangeRate = message->actualExchangeRate();
        auto actualCommission = message->actualCommission();

        // Handle the condition change
        return handleConditionChange(
            mCurrentAmountReservingPathIdentifier,
            actualExchangeRate,
            actualCommission);
    }

    if (message->state() != CoordinatorReservationResponseMessage::Accepted) {
        return reject("Unexpected message state. Protocol error. Transaction closed.");
    }

    if (message->amountReserved() == 0) {
        warning() << "Neighbor node doesn't accepted coordinator request regarding to 0 amount.";
        dropReservationsOnPath(
            currentAmountReservationPathStats(),
            mCurrentAmountReservingPathIdentifier);
        // processed trustLine was rejected, we add it to Rejected TrustLines
        const auto kPathStats = currentAmountReservationPathStats();
        const auto neighborAddressAndPos = kPathStats->currentIntermediateNodeAndPos();
        const auto& pathNodes = kPathStats->path().nodes;

        if (static_cast<size_t>(neighborAddressAndPos.second + 1) >= pathNodes.size()) {
            warning() << "Invalid path position for next neighbor";
            return tryProcessNextPath();
        }

        const auto nextNeighborAddress = pathNodes[neighborAddressAndPos.second + 1];
        mRejectedTrustLines.emplace_back(
            neighborAddressAndPos.first,
            nextNeighborAddress);
        // sending message to receiver that transaction continues
        sendMessage<TTLProlongationResponseMessage>(
            mContractor->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            TTLProlongationResponseMessage::Continue);
        return tryProcessNextPath();
    }

    auto path = currentAmountReservationPathStats();
    path->setNodeState(
        kFirstIntermediateNodeIndex,
        OptimalPathResult::ReservationApproved);
    debug() << "Neighbor node accepted coordinator request. Reserved: " << message->amountReserved();
    mCurrentPathParticipants.push_back(
        make_shared<Contractor>(
            message->senderAddresses));

    const auto pathFlow = path->previousPathFlow();
    if (message->amountReserved() != pathFlow.first) {
        shortageReservationsOnPath(
            neighborID,
            mCurrentAmountReservingPathIdentifier,
            message->amountReserved());
        debug() << "Path max flow is now " << message->amountReserved();
    }

    if (path->isLastIntermediateNodeProcessed()) {

        // Get sender equivalent to check total reserved amount
        // TODO: we should take first path equivalent, because it is the one that was used to reserve the amount
        // but if we will have more than one exchangeEquivalent, we should change this logic
        const auto senderEquivalent = path->currentPathEquivalent();
        const auto kTotalAmount = totalReservedAmount(
                                      AmountReservation::Outgoing, senderEquivalent);

        debug() << "Current path reservation finished";
        debug() << "Total collected amount by all paths: " << kTotalAmount;

        if (kTotalAmount > mExchangeAmount) {
            info() << "Total exchange amount: " << mExchangeAmount;
            return reject("Total collected amount is greater than exchange amount. "
                          "It indicates that some of the nodes doesn't follows the protocol, "
                          "or that an error is present in protocol itself.");
        }

        try {
            addFinalConfigurationOnPath(
                mCurrentAmountReservingPathIdentifier,
                path);
        } catch (const ValueError& e) {
            error() << "Failed to add final configuration: " << e.what();
            return reject("Internal payment error: flow calculation mismatch");
        }

        // Check maxAllowablePaymentAmount after path completion
        TrustLineAmount totalReserved = calculateTotalReservedPaymentAmount();

        if (exceedsAllowablePaymentAmount(totalReserved)) {
            warning() << "Total reserved payment amount " << totalReserved
                      << " exceeds maximum allowable payment amount "
                      << mCommand->maxAllowablePaymentAmount()
                      << " after processing path " << mCurrentAmountReservingPathIdentifier << " via neighbor";

            // Call reject() without return - triggers rollBack() in base class
            reject("Allowable payment amount exceeded");

            // Return specific error code
            return resultAllowablePaymentAmountExceeded();
        }

        debug() << "Total reserved amount " << totalReserved
                << " within allowable limit " << mCommand->maxAllowablePaymentAmount();

        // do not need to send final path exchange configuration message,
        // because this path contains only one intermediate node and it already has final configuration

        if (kTotalAmount == mExchangeAmount) {
            debug() << "Total exchange amount: " << mExchangeAmount << ". Collected.";

            mStep = Common_ObservingBlockNumberProcessing;
            mResourcesManager->requestObservingBlockNumber(
                mTransactionUUID);
            return resultWaitForResourceTypes(
            {BaseResource::ObservingBlockNumber},
            maxNetworkDelay(1));
        }
        return tryProcessNextPath();
    }

    // todo use return tryReserveNextIntermediateNodeAmount(path);
    return runAmountReservationStage();
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::processRemoteNodeResponse()
{
    debug() << "processRemoteNodeResponse";
    if (!contextIsValid(Message::Payments_CoordinatorReservationResponse)) {
        dropReservationsOnPath(
            currentAmountReservationPathStats(),
            mCurrentAmountReservingPathIdentifier,
            true);
        // sending message to receiver that transaction continues
        sendMessage<TTLProlongationResponseMessage>(
            mContractor->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            TTLProlongationResponseMessage::Continue);
        debug() << "Switching to another path.";

        // remote node is inaccessible, we add it to offline nodes
        const auto kPathStats = currentAmountReservationPathStats();
        const auto addressAndPos = kPathStats->currentIntermediateNodeAndPos();
        mInaccessibleNodes.push_back(addressAndPos.first);
        debug() << addressAndPos.first->fullAddress() << " was added to offline nodes";

        return tryProcessNextPath();
    }

    const auto message = popNextMessage<CoordinatorReservationResponseMessage>();
    auto remoteNodeAddress = message->senderAddresses.at(0);
    info() << "Remote node " << remoteNodeAddress->fullAddress() << " sent response";
    // todo: check message sender

    if (message->pathID() != mCurrentAmountReservingPathIdentifier) {
        warning() << "Remote node sen response on wrong path " << message->pathID()
                  << " . Continue previous state";
        return resultContinuePreviousState();
    }

    if (message->state() == CoordinatorReservationResponseMessage::Closed) {
        return reject("Desynchronization in reservation with Receiver occurred. Transaction closed.");
    }

    auto path = currentAmountReservationPathStats();
    auto remoteNodeAndPos = path->currentIntermediateNodeAndPos();
    const auto& pathNodes = path->path().nodes;

    if (message->state() == CoordinatorReservationResponseMessage::NextNodeInaccessible) {
        warning() << "Next node after remote node is inaccessible. Rejecting request.";
        dropReservationsOnPath(
            currentAmountReservationPathStats(),
            mCurrentAmountReservingPathIdentifier);

        // next after remote node is inaccessible, we add it to offline nodes
        if (static_cast<size_t>(remoteNodeAndPos.second + 1) >= pathNodes.size()) {
            warning() << "Invalid path position";
            return tryProcessNextPath();
        }

        const auto nextAfterRemoteNode = pathNodes[remoteNodeAndPos.second + 1];
        if (nextAfterRemoteNode == mContractor->mainAddress()) {
            mCountReceiverInaccessible++;
            if (mCountReceiverInaccessible >= kMaxReceiverInaccessible) {
                reject("Contractor is offline. Rollback.");
                return resultNoResponseError();
            }
        } else {
            mInaccessibleNodes.push_back(nextAfterRemoteNode);
            debug() << nextAfterRemoteNode->fullAddress() << " was added to offline nodes";
        }

        // sending message to receiver that transaction continues
        sendMessage<TTLProlongationResponseMessage>(
            mContractor->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            TTLProlongationResponseMessage::Continue);

        return tryProcessNextPath();
    }

    /*
     * Nodes scheme:
     * R - remote node;
     */
    if (static_cast<size_t>(remoteNodeAndPos.second + 1) >= pathNodes.size()) {
        warning() << "Invalid path position for next node";
        return tryProcessNextPath();
    }
    auto nextAfterRemoteNode = pathNodes[remoteNodeAndPos.second + 1];

    if (message->state() == CoordinatorReservationResponseMessage::RejectedDueOwnKeysAbsence or
            message->state() == CoordinatorReservationResponseMessage::RejectedDueContractorKeysAbsence) {
        warning() << "Remote node doesn't accepted coordinator request due to keys absence. Switching to another path.";
        dropReservationsOnPath(
            currentAmountReservationPathStats(),
            mCurrentAmountReservingPathIdentifier);
        mRejectedTrustLines.emplace_back(
            remoteNodeAndPos.first,
            nextAfterRemoteNode);
        mParticipantsKeysProblem = true;
        // sending message to receiver that transaction continues
        sendMessage<TTLProlongationResponseMessage>(
            mContractor->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            TTLProlongationResponseMessage::Continue);
        return tryProcessNextPath();
    }

    if (message->state() == CoordinatorReservationResponseMessage::RejectedDueAuditPending) {
        warning() << "Remote node doesn't accepted coordinator request due to audit pending. Switching to another path.";
        dropReservationsOnPath(
            currentAmountReservationPathStats(),
            mCurrentAmountReservingPathIdentifier);
        mRejectedTrustLines.emplace_back(
            remoteNodeAndPos.first,
            nextAfterRemoteNode);
        mIsAuditPendingPathsOccurred = true;
        // sending message to receiver that transaction continues
        sendMessage<TTLProlongationResponseMessage>(
            mContractor->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            TTLProlongationResponseMessage::Continue);
        return tryProcessNextPath();
    }

    // Handle condition change rejection (exchange rate or commission mismatch)
    if (message->state() == CoordinatorReservationResponseMessage::RejectedDueConditionsChanged) {
        info() << "Condition change detected from remote node on pathID=" << mCurrentAmountReservingPathIdentifier;

        // Extract actual conditions from response
        auto actualExchangeRate = message->actualExchangeRate();
        auto actualCommission = message->actualCommission();

        // Handle the condition change
        return handleConditionChange(
            mCurrentAmountReservingPathIdentifier,
            actualExchangeRate,
            actualCommission);
    }

    if (0 == message->amountReserved() || message->state() == CoordinatorReservationResponseMessage::Rejected) {
        warning() << "Remote node rejected reservation. Switching to another path.";
        dropReservationsOnPath(
            currentAmountReservationPathStats(),
            mCurrentAmountReservingPathIdentifier);
        // processed trustLine was rejected, we add it to Rejected TrustLines
        mRejectedTrustLines.emplace_back(
            remoteNodeAndPos.first,
            nextAfterRemoteNode);
        // sending message to receiver that transaction continues
        sendMessage<TTLProlongationResponseMessage>(
            mContractor->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            TTLProlongationResponseMessage::Continue);

        path->setUnusable();
        path->setNodeState(
            remoteNodeAndPos.second,
            OptimalPathResult::ReservationRejected);

        return tryProcessNextPath();
    }

    if (message->state() != CoordinatorReservationResponseMessage::Accepted) {
        return reject("Unexpected message state. Protocol error. Transaction closed.");
    }

    const auto reservedAmount = message->amountReserved();
    debug() << "Remote node reserved " << reservedAmount;

    mCurrentPathParticipants.push_back(
        make_shared<Contractor>(
            message->senderAddresses));
    path->setNodeState(
        remoteNodeAndPos.second,
        OptimalPathResult::ReservationApproved);

    const auto pathFlow = path->previousPathFlow();
    if (reservedAmount != pathFlow.first) {
        auto firstIntermediateNode = pathNodes[0];
        auto firstIntermediateNodeID = mContractorsManager->contractorIDByAddress(firstIntermediateNode);
        shortageReservationsOnPath(
            firstIntermediateNodeID,
            mCurrentAmountReservingPathIdentifier,
            reservedAmount);
        debug() << "Path max flow is now " << reservedAmount;
    }

    if (path->isLastIntermediateNodeProcessed()) {

        // Get sender equivalent from path to check total reserved amount
        // TODO: we should take first path equivalent, because it is the one that was used to reserve the amount
        // but if we will have more than one exchangeEquivalent, we should change this logic
        const auto senderEquivalent = path->currentPathEquivalent();
        const auto kTotalAmount = totalReservedAmount(
                                      AmountReservation::Outgoing, senderEquivalent);

        debug() << "Current path reservation finished";
        debug() << "Total collected amount by all paths: " << kTotalAmount;

        if (kTotalAmount > mExchangeAmount) {
            debug() << "Total exchange amount: " << mExchangeAmount;
            return reject("Total collected amount is greater than exchange amount. "
                          "It indicates that some of the nodes doesn't follows the protocol, "
                          "or that an error is present in protocol itself.");
        }


        try {
            addFinalConfigurationOnPath(
                mCurrentAmountReservingPathIdentifier,
                path);
        } catch (const ValueError& e) {
            error() << "Failed to add final configuration: " << e.what();
            return reject("Internal payment error: flow calculation mismatch");
        }

        // Check maxAllowablePaymentAmount after path completion
        TrustLineAmount totalReserved = calculateTotalReservedPaymentAmount();

        if (exceedsAllowablePaymentAmount(totalReserved)) {
            warning() << "Total reserved payment amount " << totalReserved
                      << " exceeds maximum allowable payment amount "
                      << mCommand->maxAllowablePaymentAmount()
                      << " after processing path " << mCurrentAmountReservingPathIdentifier;

            // Call reject() without return - triggers rollBack() in base class
            reject("Allowable payment amount exceeded");

            // Return specific error code
            return resultAllowablePaymentAmountExceeded();
        }

        debug() << "Total reserved amount " << totalReserved
                << " within allowable limit " << mCommand->maxAllowablePaymentAmount();

        // send final path configuration to all intermediate nodes on path
        sendFinalPathConfiguration(
            path,
            mCurrentAmountReservingPathIdentifier);

        if (kTotalAmount == mExchangeAmount) {
            debug() << "Total exchange amount: " << mExchangeAmount << ". Collected.";

            mStep = Common_ObservingBlockNumberProcessing;
            mResourcesManager->requestObservingBlockNumber(
                mTransactionUUID);
            return resultWaitForResourceTypes(
            {BaseResource::ObservingBlockNumber},
            maxNetworkDelay(1));
        }
        return tryProcessNextPath();
    }

    return tryReserveNextIntermediateNodeAmount(path);
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::tryProcessNextPath()
{
    debug() << "tryProcessNextPath";

    // Check if more capacity needed before taking next path
    TrustLineAmount totalReserved = calculateTotalReservedAmount();
    if (totalReserved > mAmount) {
        debug() << "Total amount: " << mAmount;
        return reject("Total collected amount is greater than amount. "
                      "It indicates that some of the nodes doesn't follows the protocol, "
                      "or that an error is present in protocol itself.");
    }
    if (totalReserved == mAmount) {
        // Sufficient capacity reserved, no more paths needed
        info() << "Path filtering: sufficient capacity already reserved ("
               << totalReserved << " >= " << mAmount << "), proceeding to next stage";

        // Request observing block number resource and transition to next stage
        mResourcesManager->requestObservingBlockNumber(mTransactionUUID);
        mStep = Stages::Common_ObservingBlockNumberProcessing;

        return resultWaitForResourceTypes(
            {BaseResource::ObservingBlockNumber},
            maxNetworkDelay(1));
    }

    // Switch to next path
    try {
        switchToNextPath();
    } catch (NotFoundError &e) {
        debug() << "No another paths are available. Try build new paths.";
        mRebuildingAttemptsCount++;
        if (mRebuildingAttemptsCount > kMaxRebuildingAttemptsCount) {
            reject("Count rebuilding attempts reaches maximal number. Canceling.");
            return resultInsufficientFundsError();
        }

        if (mInaccessibleNodes.size() != mPreviousInaccessibleNodesCount ||
                mRejectedTrustLines.size() != mPreviousRejectedTrustLinesCount) {
            auto countPathsBeforeBuilding = mPathsStats.size();
            buildPathsAgain();

            if (mPathsStats.size() > countPathsBeforeBuilding) {
                const TrustLineAmount totalReservedAfterRebuild = calculateTotalReservedAmount();
                const TrustLineAmount remainingReceive = mAmount - totalReservedAfterRebuild;
                const TrustLineAmount totalCapacity = calculateTotalPathCapacityForReceive();
                info() << "Rebuilt paths capacity check: remainingReceive=" << remainingReceive
                       << ", totalCapacity=" << totalCapacity;

                if (totalCapacity < remainingReceive) {
                    warning() << "Rebuilt paths insufficient capacity: remainingReceive=" << remainingReceive
                              << ", totalCapacity=" << totalCapacity;
                    reject("Rebuilt paths have insufficient capacity");
                    return resultInsufficientFundsError();
                }

                info() << "Rebuilt paths capacity is sufficient; resuming reservation stage";
                debug() << "New paths was built " << to_string(mPathsStats.size() - countPathsBeforeBuilding);
                mPreviousInaccessibleNodesCount = mInaccessibleNodes.size();
                mPreviousRejectedTrustLinesCount = mRejectedTrustLines.size();
                // in case if amount on direct paths changed, we can process it again
                mDirectPathIsAlreadyProcessed = false;
                if (mTestShortCircuitAfterCapacityValidation) {
                    return resultAwakeAsFastAsPossible();
                }
                initAmountsReservationOnNextPath();
                mIsAuditPendingPathsOccurred = false;
                return runAmountReservationStage();
            }
            debug() << "New paths was not built";
        }

        if (mIsAuditPendingPathsOccurred) {
            debug() << "try to build new paths due to audit pending TLs";
            auto countPathsBeforeBuilding = mPathsStats.size();
            mRejectedTrustLines.clear();
            buildPathsAgain();

            if (mPathsStats.size() > countPathsBeforeBuilding) {
                const TrustLineAmount totalReservedAfterRebuild = calculateTotalReservedAmount();
                const TrustLineAmount remainingReceive = mAmount - totalReservedAfterRebuild;
                const TrustLineAmount totalCapacity = calculateTotalPathCapacityForReceive();
                info() << "Rebuilt paths capacity check: remainingReceive=" << remainingReceive
                       << ", totalCapacity=" << totalCapacity;

                if (totalCapacity < remainingReceive) {
                    warning() << "Rebuilt paths insufficient capacity after audit retry: remainingReceive="
                              << remainingReceive << ", totalCapacity=" << totalCapacity;
                    reject("Rebuilt paths have insufficient capacity");
                    return resultInsufficientFundsError();
                }

                info() << "Rebuilt paths capacity is sufficient after audit retry; resuming reservation stage";
                debug() << "New paths was built " << to_string(mPathsStats.size() - countPathsBeforeBuilding);
                mPreviousInaccessibleNodesCount = mInaccessibleNodes.size();
                mPreviousRejectedTrustLinesCount = mRejectedTrustLines.size();
                // in case if amount on direct paths changed, we can process it again
                mDirectPathIsAlreadyProcessed = false;
                if (mTestShortCircuitAfterCapacityValidation) {
                    return resultAwakeAsFastAsPossible();
                }
                initAmountsReservationOnNextPath();
                mIsAuditPendingPathsOccurred = false;
                return resultWaitForMessageTypes( {
                    Message::Payments_IntermediateNodeReservationResponse,
                    Message::Payments_TTLProlongationRequest,
                    Message::General_NoEquivalent},
                kAuditRetryingIntervalInMilliseconds);
            }
            debug() << "New paths was not built";
        }
        reject("No another paths are available. Canceling.");
        return resultInsufficientFundsError();
    }

    // Continue with next path
    mStep = Stages::Coordinator_AmountReservation;
    mReservationsStage = 0;

    return runAmountReservationStage();
}

void CoordinatorExchangePaymentTransaction::switchToNextPath()
{
    auto justProcessedPathIdentifier = mCurrentAmountReservingPathIdentifier;
    auto justProcessedPath = currentAmountReservationPathStats();

    if (!mPathIDs.empty()) {
        mPathIDs.erase(mPathIDs.cbegin());
    }

    // Remove unusable path from paths scope
    if (!justProcessedPath->isValid()) {
        releasePathCommissions(justProcessedPathIdentifier, true);
        mPathsStats.erase(justProcessedPathIdentifier);
    }

    // Step 1: Check if more capacity needed before taking next path
    TrustLineAmount totalReserved = calculateTotalReservedAmount();
    TrustLineAmount remainingNeeded = mAmount - totalReserved;
    debug() << "Path filtering: remaining needed = " << remainingNeeded
            << " (total reserved: " << totalReserved << ", target: " << mAmount << ")";

    // Step 2: Find valid path through filtering
    while (!mPathIDs.empty()) {
        PathID nextPathID = *mPathIDs.cbegin();
        auto pathStatsIt = mPathsStats.find(nextPathID);

        if (pathStatsIt == mPathsStats.end()) {
            warning() << "Path filtering: next path not found in mPathsStats, pathID=" << nextPathID;
            mPathIDs.erase(mPathIDs.cbegin());
            continue;
        }

        OptimalPathResult *pathStats = pathStatsIt->second.get();

        // Step 2.5: Check if path is valid (may have been invalidated by condition changes)
        if (!pathStats->isValid()) {
            warning() << "Path filtering: path " << nextPathID << " is invalid, skipping";
            releasePathCommissions(nextPathID, true);
            mPathsStats.erase(nextPathID);
            mPathIDs.erase(mPathIDs.cbegin());
            continue;
        }

        // Step 3: Validate path for processing (check inaccessible nodes and rejected trust lines)
        if (!validatePathForProcessing(pathStats)) {
            // Path contains bad nodes/trustlines, mark unusable and try next
            pathStats->setUnusable();
            releasePathCommissions(nextPathID, true);
            mPathsStats.erase(nextPathID);
            mPathIDs.erase(mPathIDs.cbegin());
            continue;
        }

        // Step 3.5: Adjust commissions for this path based on previous consumption
        prepareCommissionsForPath(pathStats, nextPathID);

        // Step 4: Check if truncation needed
        if (pathStats->received_amount > remainingNeeded) {
            info() << "Path capacity truncation: available=" << pathStats->received_amount
                   << ", needed=" << remainingNeeded;

            try {
                // Calculate truncated input amount that delivers exactly remainingNeeded
                TrustLineAmount truncatedInput = calculateRequiredInputForPath(
                    *pathStats, remainingNeeded);

                // Update path capacity
                pathStats->optimal_flow = truncatedInput;
                pathStats->received_amount = remainingNeeded;

                // Recalculate flows for reservation
                pathStats->calculateFlows(truncatedInput);

                // Re-apply commission adjustments after recalculation
                prepareCommissionsForPath(pathStats, nextPathID);

                info() << "Path capacity truncated: input=" << pathStats->optimal_flow
                       << ", output=" << pathStats->received_amount;

            } catch (const std::exception &e) {
                // Truncation calculation failed, skip this path and try next
                error() << "Path capacity truncation failed: " << e.what()
                        << ", skipping path";
                pathStats->setUnusable();
                releasePathCommissions(nextPathID, true);
                mPathsStats.erase(nextPathID);
                mPathIDs.erase(mPathIDs.cbegin());
                continue;
            }
        }

        // Step 5: Path is valid and truncated if needed, proceed with it
        mCurrentAmountReservingPathIdentifier = nextPathID;
        debug() << "[" << mCurrentAmountReservingPathIdentifier << "] switching to next path";
        return;
    }

    // No more paths available
    throw NotFoundError(
        "CoordinatorExchangePaymentTransaction::switchToNextPath: "
        "no paths are available");
}

void CoordinatorExchangePaymentTransaction::informAllNodesAboutTransactionFinish()
{
    debug() << "informAllNodesAboutTransactionFinish";
    for (auto const &paymentNodeIdAndContractor : mPaymentParticipants) {
        if (paymentNodeIdAndContractor.first == kCoordinatorPaymentNodeID) {
            continue;
        }
        sendMessage<TTLProlongationResponseMessage>(
            paymentNodeIdAndContractor.second->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            TTLProlongationResponseMessage::Finish);
        debug() << "Send transaction finishing message to participant " << paymentNodeIdAndContractor.first;
    }
}

void CoordinatorExchangePaymentTransaction::shortageReservationsOnPath(
    ContractorID neighborID,
    const PathID &pathID,
    const TrustLineAmount &kNewAmount)
{
    debug() << "shortageReservationsOnPath: pathID=" << pathID
            << ", neighborID=" << neighborID
            << ", newAmount=" << kNewAmount;

    // Step 1: Update coordinator's own reservation (existing logic)
    auto nodeReservations = mReservations[neighborID];
    for (const auto &pathIDAndReservation : nodeReservations) {
        if (pathIDAndReservation.first == pathID) {
            // Get equivalent from reservation
            const auto equivalent = pathIDAndReservation.second->equivalent();
            shortageReservation(
                neighborID,
                pathIDAndReservation.second,
                kNewAmount,
                pathID,
                equivalent);
            // coordinator has only one reservation on each path
            break;
        }
    }

    // Step 2: Find path in mPathsStats
    auto pathStatsIt = mPathsStats.find(pathID);
    if (pathStatsIt == mPathsStats.end()) {
        warning() << "Path not found in mPathsStats for pathID=" << pathID;
        return;
    }

    OptimalPathResult *pathStats = pathStatsIt->second.get();
    const auto &path = pathStats->path();

    // Store old received_amount for logging
    const TrustLineAmount oldReceivedAmount = pathStats->received_amount;

    // Step 3: Calculate new received_amount by forward-simulating through path
    // The kNewAmount is the amount coordinator sends to first node (in sender equivalent)
    // We need to calculate what receiver gets (in receiver equivalent)
    TrustLineAmount newReceivedAmount;
    try {
        // Forward simulate through path: apply exchanges and subtract commissions
        // Note: We use path.ids and path.equivalents for iteration (NOT path.nodes)
        // because OptimalPathResult::findExchangeStep expects ContractorID, not BaseAddress

        TrustLineAmount currentAmount = kNewAmount;

        for (size_t idx = 0; idx + 1 < path.ids.size(); ++idx) {
            const ContractorID fromNode = path.ids[idx];
            const ContractorID toNode = path.ids[idx + 1];
            const SerializedEquivalent currentEquiv = path.equivalents[idx];
            const SerializedEquivalent nextEquiv = path.equivalents[idx + 1];

            // Check for exchange (same node, different equivalent)
            if (fromNode == toNode && currentEquiv != nextEquiv) {
                // Find exchange step
                const auto *exchangeStep = pathStats->findExchangeStep(
                    fromNode,
                    currentEquiv,
                    nextEquiv);

                if (!exchangeStep) {
                    warning() << "Exchange step not found during shortage calculation";
                    return;  // Keep old values
                }

                // Apply exchange forward
                currentAmount = exchangeStep->applyExchangeForward(currentAmount);
                continue;  // Don't add to flows, this is in-place exchange
            }

            // Check for commission at arrival node (if not receiver)
            if (idx + 1 < path.ids.size() - 1) {  // Not last node (receiver)
                const auto *commissionStep = pathStats->findExchangeStep(
                    toNode,
                    nextEquiv,
                    nextEquiv);

                if (commissionStep && commissionStep->commission > TrustLineAmount(0)) {
                    if (currentAmount < commissionStep->commission) {
                        warning() << "Amount exhausted by commission during shortage, "
                                  << "marking path unusable";
                        pathStats->setUnusable();
                        return;
                    }
                    currentAmount = currentAmount - commissionStep->commission;
                }
            }
        }

        newReceivedAmount = currentAmount;

    } catch (const std::exception &e) {
        warning() << "Error calculating new received amount: " << e.what();
        return;  // Keep old values
    }

    // Step 4: Update ALL path statistics fields
    // During shortage (capacity reduction from intermediate node), we update both
    // mMaxPathFlow and mMaxPathReceivedAmount because the actual maximum capacities
    // on both sender and receiver sides have been reduced.
    // This is different from handleConditionChange where these bounds are preserved.
    pathStats->mMaxPathFlow = kNewAmount;
    pathStats->mMaxPathReceivedAmount = newReceivedAmount;
    pathStats->optimal_flow = kNewAmount;
    pathStats->received_amount = newReceivedAmount;

    // Step 5: Recalculate flows vector
    try {
        pathStats->calculateFlows(kNewAmount);
    } catch (const std::exception &e) {
        warning() << "Error recalculating flows: " << e.what();
        // flows may be inconsistent, but main fields are updated
    }

    info() << "Path capacity adjusted: pathID=" << pathID
           << ", newMaxFlow=" << kNewAmount
           << ", newOptimalFlow=" << kNewAmount
           << ", newReceivedAmount=" << newReceivedAmount
           << " (was " << oldReceivedAmount << ")"
           << ", flows recalculated";
}

void CoordinatorExchangePaymentTransaction::dropReservationsOnPath(
    OptimalPathResult *pathStats,
    const PathID &pathID,
    bool sendToLastProcessedNode)
{
    debug() << "dropReservationsOnPath";
    releasePathCommissions(pathID, true);
    pathStats->setUnusable();

    if (pathStats->mPath.nodes.empty()) {
        warning() << "Path has no nodes, cannot drop reservations";
        return;
    }

    auto firstIntermediateNode = pathStats->mPath.nodes[0];
    auto firstIntermediateNodeID = mContractorsManager->contractorIDByAddress(firstIntermediateNode);

    if (firstIntermediateNodeID == ContractorsManager::kNotFoundContractorID) {
        warning() << "First intermediate node not found in contractors";
        return;
    }

    auto nodeReservations = mReservations.find(firstIntermediateNodeID);
    if (nodeReservations == mReservations.end()) {
        debug() << "No reservations found for first intermediate node";
        return;
    }

    // Determine equivalences based on precomputed flows
    const auto &pathFlows = pathStats->flows;
    const auto senderEquivalent = pathFlows.empty()
        ? mEquivalent
        : pathFlows.front().second;
    auto senderTrustLines = trustLinesManager(senderEquivalent);

    auto itPathIDAndReservation = nodeReservations->second.begin();
    while (itPathIDAndReservation != nodeReservations->second.end()) {
        if (itPathIDAndReservation->first == pathID) {
            debug() << "Dropping reservation: [ => ] " << itPathIDAndReservation->second->amount()
                    << " for (" << firstIntermediateNode->fullAddress() << ") [" << pathID << "]";
            senderTrustLines->dropAmountReservation(
                firstIntermediateNodeID,
                itPathIDAndReservation->second);

            itPathIDAndReservation = nodeReservations->second.erase(itPathIDAndReservation);
            // coordinator has only one reservation on each path
            break;
        }
        else {
            itPathIDAndReservation++;
        }
    }
    if (nodeReservations->second.empty()) {
        mReservations.erase(firstIntermediateNodeID);
    }

    // send message with dropping reservation instruction to all intermediate nodes because this path is unusable
    if (pathStats->path().length() == 1) {
        return;
    }

    try {
        const auto lastProcessedNodeAndPos = pathStats->currentIntermediateNodeAndPos();
        const auto lastProcessedNode = lastProcessedNodeAndPos.first;

        // Determine flows-based equivalents for all nodes on path
        const auto &pathNodes = pathStats->path().nodes;

        if (pathFlows.empty()) {
            warning() << "Path has no flows, cannot drop reservations";
            return;
        }

        if (pathFlows.size() != pathNodes.size()) {
            warning() << "Path flows size mismatch: " << pathFlows.size()
                      << " vs nodes size: " << pathNodes.size();
            return;
        }

        for (size_t nodeIdx = 0; nodeIdx < pathNodes.size(); ++nodeIdx) {
            const auto &intermediateNode = pathNodes[nodeIdx];

            if (!sendToLastProcessedNode && intermediateNode == lastProcessedNode) {
                break;
            }

            // Determine incoming and outgoing equivalents using flows
            SerializedEquivalent incomingEquivalent = pathFlows[nodeIdx].second;
            SerializedEquivalent outgoingEquivalent =
                (nodeIdx + 1 < pathFlows.size())
                    ? pathFlows[nodeIdx + 1].second
                    : incomingEquivalent;

            debug() << "send message with drop reservation info for node "
                    << intermediateNode->fullAddress()
                    << " (incoming equiv: " << incomingEquivalent
                    << ", outgoing equiv: " << outgoingEquivalent << ")";

            sendMessage<FinalPathExchangeConfigurationMessage>(
                intermediateNode,
                senderEquivalent,
                mContractorsManager->ownAddresses(),
                currentTransactionUUID(),
                pathID,
                TrustLine::kZeroAmount(),
                incomingEquivalent,
                TrustLine::kZeroAmount(),
                outgoingEquivalent);

            if (sendToLastProcessedNode && intermediateNode == lastProcessedNode) {
                break;
            }
        }
    } catch (NotFoundError &) {
        debug() << "No processed nodes yet, skipping final path messages";
    }
}

void CoordinatorExchangePaymentTransaction::savePaymentOperationIntoHistory(
    IOTransaction::Shared ioTransaction)
{
    debug() << "savePaymentOperationIntoHistory";
    // For exchange transactions, use receiver equivalent (mEquivalent) for balance calculation
    auto receiverTrustLinesManager = trustLinesManager(mEquivalent);
    ioTransaction->historyStorage()->savePaymentRecord(
        make_shared<PaymentRecord>(
            mEquivalent,
            currentTransactionUUID(),
            PaymentRecord::OutgoingPaymentType,
            mContractor,
            mCommittedAmount,
            *receiverTrustLinesManager->totalBalance().get(),
            mOutgoingTransfers,
            mIncomingTransfers,
            mCommand->UUID(),
            mCommand->payload()));
    debug() << "Operation saved";
}

void CoordinatorExchangePaymentTransaction::buildPathsAgain()
{
    debug() << "buildPathsAgain";
    auto startTime = utc_now();
    
    // Collect every equivalent that participates in the exchange (including the receiver's).
    // The vector is deduplicated to avoid redundant topology traversals when the receiver
    // equivalent is already present in the exchange list.
    std::vector<SerializedEquivalent> equivalentsToSanitize = mCommand->exchangeEquivalents();
    equivalentsToSanitize.push_back(mCommand->equivalent());
    std::sort(equivalentsToSanitize.begin(), equivalentsToSanitize.end());
    equivalentsToSanitize.erase(
        std::unique(equivalentsToSanitize.begin(), equivalentsToSanitize.end()),
        equivalentsToSanitize.end());

    // Resolve contractor IDs for inaccessible nodes upfront so we can reuse them for each equivalent.
    struct InaccessibleNodeDescriptor {
        BaseAddress::Shared address;
        optional<ContractorID> contractorID;
    };

    std::vector<InaccessibleNodeDescriptor> inaccessibleNodes;
    inaccessibleNodes.reserve(mInaccessibleNodes.size());

    for (const auto &nodeAddress : mInaccessibleNodes) {
        InaccessibleNodeDescriptor descriptor{nodeAddress, std::nullopt};
        if (nodeAddress) {
            const auto contractorID = mEquivalentsSubsystemsRouter->resolveParticipantID(nodeAddress);
            if (!contractorID) {
                warning() << "buildPathsAgain: unable to resolve ContractorID for inaccessible node "
                          << nodeAddress->fullAddress();
            } else {
                descriptor.contractorID = contractorID;
            }
        } else {
            warning() << "buildPathsAgain: encountered empty address in mInaccessibleNodes";
        }
        inaccessibleNodes.push_back(descriptor);
    }

    // Prepare descriptors for rejected trust lines in the same manner.
    struct RejectedTrustLineDescriptor {
        BaseAddress::Shared sourceAddress;
        BaseAddress::Shared targetAddress;
        optional<ContractorID> sourceID;
        optional<ContractorID> targetID;
    };

    std::vector<RejectedTrustLineDescriptor> rejectedTrustLines;
    rejectedTrustLines.reserve(mRejectedTrustLines.size());

    for (const auto &[source, target] : mRejectedTrustLines) {
        RejectedTrustLineDescriptor descriptor{source, target, std::nullopt, std::nullopt};
        if (source) {
            descriptor.sourceID = mEquivalentsSubsystemsRouter->resolveParticipantID(source);
            if (!descriptor.sourceID) {
                warning() << "buildPathsAgain: unable to resolve ContractorID for rejected TL source "
                          << source->fullAddress();
            }
        } else {
            warning() << "buildPathsAgain: encountered empty source address in mRejectedTrustLines";
        }

        if (target) {
            descriptor.targetID = mEquivalentsSubsystemsRouter->resolveParticipantID(target);
            if (!descriptor.targetID) {
                warning() << "buildPathsAgain: unable to resolve ContractorID for rejected TL target "
                          << target->fullAddress();
            }
        } else {
            warning() << "buildPathsAgain: encountered empty target address in mRejectedTrustLines";
        }

        rejectedTrustLines.push_back(descriptor);
    }

    // We only need participant IDs when we actually sanitise inaccessible nodes.
    const auto participantsIDs = mInaccessibleNodes.empty()
        ? std::vector<ContractorID>{}
        : mEquivalentsSubsystemsRouter->participantsIDs();

    for (const auto equivalent : equivalentsToSanitize) {
        TopologyTrustLinesManager *topologyManager = nullptr;
        try {
            topologyManager = mEquivalentsSubsystemsRouter->topologyTrustLineManager(equivalent);
        } catch (const std::exception &e) {
            warning() << "buildPathsAgain: failed to obtain topology manager for equivalent "
                      << equivalent << ": " << e.what();
            continue;
        }

        if (topologyManager == nullptr) {
            warning() << "buildPathsAgain: topology manager is null for equivalent " << equivalent;
            continue;
        }

        size_t removedOutgoingEdges = 0;
        size_t removedIncomingEdges = 0;

        if (!inaccessibleNodes.empty()) {
            for (const auto &nodeDescriptor : inaccessibleNodes) {
                if (!nodeDescriptor.contractorID) {
                    continue;
                }

                const auto nodeID = *nodeDescriptor.contractorID;

                // Remove every outgoing edge sourced from the inaccessible node.
                const auto outgoingSet = topologyManager->trustLinePtrsSet(nodeID);
                if (!outgoingSet.empty()) {
                    std::vector<ContractorID> targets;
                    targets.reserve(outgoingSet.size());
                    for (auto *trustLinePtr : outgoingSet) {
                        if (trustLinePtr == nullptr) {
                            continue;
                        }
                        targets.push_back(trustLinePtr->topologyTrustLine()->targetID());
                    }
                    for (const auto targetID : targets) {
                        topologyManager->removeTrustLine(nodeID, targetID);
                        ++removedOutgoingEdges;
                    }
                }

                // Remove every incoming edge that finishes at the inaccessible node.
                for (const auto participantID : participantsIDs) {
                    if (participantID == nodeID || participantID == TopologyTrustLinesManager::kCurrentNodeID) {
                        continue;
                    }

                    const auto incomingSet = topologyManager->trustLinePtrsSet(participantID);
                    if (incomingSet.empty()) {
                        continue;
                    }

                    for (auto *trustLinePtr : incomingSet) {
                        if (trustLinePtr == nullptr) {
                            continue;
                        }
                        if (trustLinePtr->topologyTrustLine()->targetID() == nodeID) {
                            topologyManager->removeTrustLine(participantID, nodeID);
                            ++removedIncomingEdges;
                        }
                    }
                }
            }

            if (removedOutgoingEdges > 0 || removedIncomingEdges > 0) {
                info() << "buildPathsAgain: equivalent " << equivalent
                       << " sanitised due to inaccessible nodes (outgoing removed="
                       << removedOutgoingEdges << ", incoming removed="
                       << removedIncomingEdges << ")";
            }
        }

        if (!rejectedTrustLines.empty()) {
            size_t removedRejectedEdges = 0;
            std::vector<std::string> removedDescriptions;

            for (const auto &tlDescriptor : rejectedTrustLines) {
                if (!tlDescriptor.sourceID || !tlDescriptor.targetID) {
                    continue;
                }

                const auto sourceID = *tlDescriptor.sourceID;
                const auto targetID = *tlDescriptor.targetID;

                const auto candidates = topologyManager->trustLinePtrsSet(sourceID);
                bool edgeExists = false;
                for (auto *trustLinePtr : candidates) {
                    if (trustLinePtr == nullptr) {
                        continue;
                    }
                    if (trustLinePtr->topologyTrustLine()->targetID() == targetID) {
                        edgeExists = true;
                        break;
                    }
                }

                if (!edgeExists) {
                    continue;
                }

                topologyManager->removeTrustLine(sourceID, targetID);
                ++removedRejectedEdges;

                if (tlDescriptor.sourceAddress && tlDescriptor.targetAddress) {
                    std::ostringstream oss;
                    oss << tlDescriptor.sourceAddress->fullAddress()
                        << " -> "
                        << tlDescriptor.targetAddress->fullAddress();
                    removedDescriptions.push_back(oss.str());
                }
            }

            if (removedRejectedEdges > 0) {
                std::ostringstream summary;
                for (size_t idx = 0; idx < removedDescriptions.size(); ++idx) {
                    if (idx > 0) {
                        summary << ", ";
                    }
                    summary << removedDescriptions[idx];
                }

                info() << "buildPathsAgain: equivalent " << equivalent
                       << " removed " << removedRejectedEdges
                       << " rejected trust lines [" << summary.str() << "]";
            }
        }

        size_t appliedIncomingReservations = 0;
        TrustLineAmount totalIncomingReserved = TrustLineAmount(0);

        if (!mNodesFinalAmountsConfiguration.empty()) {
            for (const auto &nodeConfig : mNodesFinalAmountsConfiguration) {
                const auto paymentNodeIdIt = mPaymentNodesIds.find(nodeConfig.first);
                if (paymentNodeIdIt == mPaymentNodesIds.end()) {
                    warning() << "buildPathsAgain: node " << nodeConfig.first
                              << " not present in mPaymentNodesIds";
                    continue;
                }

                const auto participantIt = mPaymentParticipants.find(paymentNodeIdIt->second);
                if (participantIt == mPaymentParticipants.end() || !participantIt->second) {
                    warning() << "buildPathsAgain: participant missing for node " << nodeConfig.first;
                    continue;
                }

                const auto nodeAddress = participantIt->second->mainAddress();
                if (!nodeAddress) {
                    warning() << "buildPathsAgain: node " << nodeConfig.first
                              << " has no main address";
                    continue;
                }

                ContractorID targetID;
                try {
                    targetID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(nodeAddress);
                } catch (const std::exception &e) {
                    warning() << "buildPathsAgain: unable to resolve ContractorID for node "
                              << nodeConfig.first << ": " << e.what();
                    continue;
                }

                for (const auto &reservation : nodeConfig.second) {
                    if (reservation.direction != PathReservation::Incoming ||
                        reservation.equivalent != equivalent) {
                        continue;
                    }

                    if (!reservation.amount) {
                        warning() << "buildPathsAgain: reservation for node " << nodeConfig.first
                                  << " has null amount";
                        continue;
                    }

                    const auto pathIt = mPathsStats.find(reservation.pathID);
                    if (pathIt == mPathsStats.end() || !(pathIt->second)) {
                        warning() << "buildPathsAgain: pathID " << reservation.pathID
                                  << " not found while applying reservation for node "
                                  << nodeConfig.first;
                        continue;
                    }

                    auto *pathStats = pathIt->second.get();
                    const int nodePosition = pathStats->path().positionOfNode(nodeAddress);
                    if (nodePosition < 0) {
                        warning() << "buildPathsAgain: node " << nodeConfig.first
                                  << " not present in path " << reservation.pathID;
                        continue;
                    }

                    if (nodePosition == 0) {
                        debug() << "buildPathsAgain: skipping reservation for node "
                                << nodeConfig.first
                                << " at position 0 (no previous hop)";
                        continue;
                    }

                    const auto &pathNodes = pathStats->path().nodes;
                    if (static_cast<size_t>(nodePosition) >= pathNodes.size()) {
                        warning() << "buildPathsAgain: inconsistent node position "
                                  << nodePosition << " for node " << nodeConfig.first
                                  << " in path " << reservation.pathID;
                        continue;
                    }

                    const auto previousNodeAddress = pathNodes.at(static_cast<size_t>(nodePosition - 1));
                    if (!previousNodeAddress) {
                        warning() << "buildPathsAgain: previous node address missing for node "
                                  << nodeConfig.first << " in path " << reservation.pathID;
                        continue;
                    }

                    ContractorID sourceID;
                    try {
                        sourceID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(previousNodeAddress);
                    } catch (const std::exception &e) {
                        warning() << "buildPathsAgain: unable to resolve ContractorID for previous node "
                                  << previousNodeAddress->fullAddress() << " in path "
                                  << reservation.pathID << ": " << e.what();
                        continue;
                    }

                    const auto outgoingEdges = topologyManager->trustLinePtrsSet(sourceID);
                    TopologyTrustLineWithPtr *matchedEdge = nullptr;
                    for (auto *edgePtr : outgoingEdges) {
                        if (edgePtr == nullptr) {
                            continue;
                        }
                        if (edgePtr->topologyTrustLine()->targetID() == targetID) {
                            matchedEdge = edgePtr;
                            break;
                        }
                    }

                    if (matchedEdge == nullptr) {
                        warning() << "buildPathsAgain: trust line " << previousNodeAddress->fullAddress()
                                  << " -> " << nodeConfig.first
                                  << " not found in topology for equivalent " << equivalent;
                        continue;
                    }

                    TrustLineAmount reservedAmount = *reservation.amount;
                    if (reservedAmount == TrustLineAmount(0)) {
                        debug() << "buildPathsAgain: skipping zero reservation on "
                                << previousNodeAddress->fullAddress() << " -> " << nodeConfig.first;
                        continue;
                    }

                    const auto hopCapacityPtr = matchedEdge->topologyTrustLine()->amount();
                    if (!hopCapacityPtr) {
                        warning() << "buildPathsAgain: trust line "
                                  << previousNodeAddress->fullAddress() << " -> " << nodeConfig.first
                                  << " has null capacity pointer";
                        continue;
                    }

                    const TrustLineAmount maxHopCapacity = *hopCapacityPtr;
                    if (reservedAmount >= maxHopCapacity) {
                        topologyManager->makeFullyUsed(sourceID, targetID);
                    } else {
                        topologyManager->addUsedAmount(sourceID, targetID, reservedAmount);
                    }

                    try {
                        totalIncomingReserved += reservedAmount;
                    } catch (const std::exception &e) {
                        warning() << "buildPathsAgain: overflow while accumulating reservation amount: "
                                  << e.what();
                    }
                    ++appliedIncomingReservations;
                }
            }
        }

        info() << "buildPathsAgain: equivalent " << equivalent
               << " applied " << appliedIncomingReservations
               << " incoming reservations, total=" << totalIncomingReserved;
    }

    // Recalculate paths only after topology has been fully sanitised across all equivalents.
    // This ensures OR-Tools sees a coherent snapshot without inaccessible nodes or exhausted edges.
    if (mExchangePathsManager == nullptr) {
        warning() << "buildPathsAgain: ExchangePathsManager is null, cannot rebuild paths";
        debug() << "buildPathsAgain method time: " << utc_now() - startTime;
        return;
    }

    // Attempt to reuse cached contractor ID; resolve again as a safety net when missing.
    if (mContractorID == 0 && mContractor) {
        try {
            mContractorID = mEquivalentsSubsystemsRouter->getOrCreateParticipantID(
                mContractor->mainAddress());
        } catch (const std::exception &e) {
            warning() << "buildPathsAgain: unable to resolve contractor ID for "
                      << mContractor->mainAddress()->fullAddress() << ": " << e.what();
        }
    }

    if (mContractorID == 0) {
        warning() << "buildPathsAgain: contractor ID remains unresolved, skipping path rebuild";
        debug() << "buildPathsAgain method time: " << utc_now() - startTime;
        return;
    }

    try {
        // Use special constant to mark the coordinator as the sender in topology graphs.
        const ContractorID senderID = TopologyTrustLinesManager::kCurrentNodeID;
        const auto hopsLimit = static_cast<uint8_t>(kMaxPathLength);

        auto maxFlowResult = mExchangePathsManager->calculateMaxFlow(
            mContractorID,
            mCommand->equivalent(),
            mCommand->exchangeEquivalents(),
            senderID,
            hopsLimit);

        info() << "buildPathsAgain: calculateMaxFlow returned "
               << maxFlowResult.optimalPaths.size() << " paths, max flow "
               << maxFlowResult.maxFlow;

        size_t successfullyRegistered = 0;

        for (const auto &rebuiltPath : maxFlowResult.optimalPaths) {
            // Skip zero-capacity paths to avoid polluting mPathsStats with unusable entries.
            if (rebuiltPath.received_amount == TrustLineAmount(0) ||
                rebuiltPath.optimal_flow == TrustLineAmount(0)) {
                debug() << "buildPathsAgain: skipping zero-capacity rebuilt path";
                continue;
            }

            const auto previousPathIDsSize = mPathIDs.size();

            try {
                addPathForFurtherProcessing(rebuiltPath, rebuiltPath.optimal_flow);
            } catch (const std::exception &e) {
                warning() << "buildPathsAgain: failed to register rebuilt path: "
                          << e.what();
                continue;
            }

            if (mPathIDs.size() <= previousPathIDsSize) {
                warning() << "buildPathsAgain: rebuilt path was not appended to path processing queue";
                continue;
            }

            const auto newPathID = mPathIDs.back();
            auto pathIt = mPathsStats.find(newPathID);
            if (pathIt == mPathsStats.end()) {
                warning() << "buildPathsAgain: path stats missing for rebuilt path " << newPathID;
                mPathIDs.pop_back();
                continue;
            }

            auto &pathStats = pathIt->second;
            debug() << "buildPathsAgain: added rebuilt path " << newPathID
                    << " with input capacity " << pathStats->optimal_flow
                    << " and receive capacity " << pathStats->received_amount;

            ++successfullyRegistered;
        }

        if (successfullyRegistered == 0) {
            warning() << "buildPathsAgain: path rebuilding produced no new usable paths";
        } else {
            info() << "buildPathsAgain: registered " << successfullyRegistered
                   << " rebuilt paths";
        }

    } catch (const std::exception &e) {
        warning() << "buildPathsAgain: calculateMaxFlow failed: " << e.what();
    }

    debug() << "buildPathsAgain method time: " << utc_now() - startTime;
}

bool CoordinatorExchangePaymentTransaction::checkReservationsDirections() const
{
    debug() << "checkReservationsDirections";
    for (const auto &nodeAndReservations : mReservations) {
        for (const auto &pathIDAndReservation : nodeAndReservations.second) {
            if (pathIDAndReservation.second->direction() != AmountReservation::Outgoing) {
                return false;
            }
        }
    }
    debug() << "All reservations directions are correct";
    return true;
}

void CoordinatorExchangePaymentTransaction::addPathForFurtherProcessing(
    const OptimalPathResult& pathResult,
    const TrustLineAmount& pathAmount)
{
    debug() << "addPathForFurtherProcessing";

    // Create mutable copy to initialize states
    auto pathCopy = make_unique<OptimalPathResult>(pathResult);

    // Step 1: Initialize nodes vector in ExchangePath (ContractorID → BaseAddress conversion)
    // Skip consecutive duplicates
    size_t pathLength = pathResult.mPath.ids.size();
    pathCopy->mPath.nodes.clear();

    for (const auto& contractorID : pathResult.mPath.ids) {
        // skip self contractor
        if (contractorID == 0) {
            continue;
        }
        auto contractor = mEquivalentsSubsystemsRouter->getParticipantAddress(contractorID);
        if (!contractor) {
            throw NotFoundError(
                "CoordinatorExchangePaymentTransaction::addPathForFurtherProcessing: "
                "Contractor not found for ID: " + to_string(contractorID));
        }
        
        // Skip consecutive duplicates - only check last element
        if (!pathCopy->mPath.nodes.empty() && 
            pathCopy->mPath.nodes.back()->fullAddress() == contractor->fullAddress()) {
            debug() << "Skipping consecutive duplicate node: " << contractor->fullAddress();
            continue;
        }
        
        pathCopy->mPath.nodes.push_back(contractor);
    }

    // Step 2: Initialize mIntermediateNodesStates with actual nodes count (after removing duplicates)
    size_t actualNodesCount = pathCopy->mPath.nodes.size();
    pathCopy->mIntermediateNodesStates.clear();
    // remove last node from intermediate nodes states
    pathCopy->mIntermediateNodesStates.resize(
        actualNodesCount - 1,
        OptimalPathResult::NodeState::ReservationRequestDoesntSent);

    // Step 3: Generate unique PathID
    PathID pathID = generateNextPathID();

    // Step 4: Add to mPathsStats and mPathIDs
    pathCopy->calculateFlows(pathAmount);

    // Initialize maximum flow and received amount constraints for this path.
    // mMaxPathFlow: maximum sender-side amount that can be paid on this path
    // mMaxPathReceivedAmount: maximum receiver-side amount that can be delivered on this path
    // Both values represent upper bounds and may be reduced during reservation processing.
    pathCopy->mMaxPathFlow = pathResult.optimal_flow;
    pathCopy->mMaxPathReceivedAmount = pathResult.received_amount;

    mPathsStats[pathID] = std::move(pathCopy);
    mPathIDs.push_back(pathID);

    debug() << "Path " << pathID << " added with "
            << pathLength << " nodes, flow: " << pathResult.optimal_flow;
    debug() << "Path " << pathID << " " << mPathsStats[pathID]->path().toString();
    for (const auto &flow : mPathsStats[pathID]->flows) {
        debug() << "Flow: " << flow.first << ", Equivalent: " << flow.second;
    }
}

PathID CoordinatorExchangePaymentTransaction::generateNextPathID()
{
    // Simple incrementing ID generator
    // If no paths exist yet, start with 1
    if (mPathsStats.empty()) {
        return 1;
    }

    // Find max existing PathID and increment
    PathID maxID = 0;
    for (const auto& [pathID, pathResult] : mPathsStats) {
        if (pathID > maxID) {
            maxID = pathID;
        }
    }

    return maxID + 1;
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::resultOK()
{
    string transactionUUID = mTransactionUUID.stringUUID();
    return transactionResultFromCommand(
               mCommand->responseOK(transactionUUID));
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::resultForbiddenRun()
{
    return transactionResultFromCommand(
               mCommand->responseForbiddenRunTransaction());
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::resultForbiddenRunDueObserving()
{
    return transactionResultFromCommand(
               mCommand->responseForbiddenRunDueObservingTransaction());
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::resultNoPathsError()
{
    return transactionResultFromCommand(
               mCommand->responseNoRoutes());
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::resultProtocolError()
{
    return transactionResultFromCommand(
               mCommand->responseProtocolError());
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::resultNoResponseError()
{
    return transactionResultFromCommand(
               mCommand->responseRemoteNodeIsInaccessible());
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::resultInsufficientFundsError()
{
    if (mNeighborsKeysProblem) {
        return transactionResultFromCommand(
                   mCommand->responseInsufficientFundsDueToKeysAbsent());
    }
    if (mParticipantsKeysProblem) {
        return transactionResultFromCommand(
                   mCommand->responseInsufficientFundsDueToParticipantsKeysAbsent());
    }
    return transactionResultFromCommand(
               mCommand->responseInsufficientFunds());
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::resultNoConsensusError()
{
    return transactionResultFromCommand(
               mCommand->responseNoConsensus());
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::resultUnexpectedError()
{
    return transactionResultFromCommand(
               mCommand->responseUnexpectedError());
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::resultAllowablePaymentAmountExceeded()
{
    return transactionResultFromCommand(
               mCommand->responseAllowablePaymentAmountExceeded());
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::approve()
{
#ifdef TESTS
    mSubsystemsController->testForbidSendMessageOnVoteConsistencyStage(
        (uint32_t)mPaymentParticipants.size() - 1);
    // participants wait for this message 6
    mSubsystemsController->testSleepOnVoteConsistencyStage(
        maxNetworkDelay(8));
    mSubsystemsController->testThrowExceptionOnCoordinatorAfterApproveBeforeSendMessage();
#endif

    for (const auto &paymentNodeIdAndContractor : mPaymentParticipants) {
        if (paymentNodeIdAndContractor.first == kCoordinatorPaymentNodeID) {
            continue;
        }
        sendMessage(
            paymentNodeIdAndContractor.second->mainAddress(),
            mParticipantsVotesMessage);
    }

    try {
        set<PathID> actualPathsIds;
        for (const auto &nodeAndReservations : mReservations) {
            for (const auto &pathIdAndReservation : nodeAndReservations.second) {
                actualPathsIds.insert(pathIdAndReservation.first);
            }
        }
        vector<vector<BaseAddress::Shared>> paymentEventPaths;
        for (const auto &identifier : actualPathsIds) {
            const auto& pathResult = mPathsStats[identifier];
            // Get intermediate nodes from ExchangePath (skip first and last nodes)
            vector<BaseAddress::Shared> intermediates;
            if (pathResult->mPath.nodes.size() > 2) {
                intermediates.assign(
                    pathResult->mPath.nodes.begin() + 1,
                    pathResult->mPath.nodes.end() - 1);
            }
            paymentEventPaths.push_back(intermediates);
        }

        mEventsInterfaceManager->writeEvent(
            Event::paymentEvent(
                mContractorsManager->selfContractor()->mainAddress(),
                mContractor->mainAddress(),
                paymentEventPaths,
                mTransactionUUID,
                mEquivalent));
    }
    catch (std::exception &e) {
        warning() << "Can't write payment event " << e.what();
    }

    mCommittedAmount = totalReservedAmount(
                           AmountReservation::Outgoing, mEquivalent);
    BaseExchangePaymentTransaction::approve();
#ifdef TESTS
    mSubsystemsController->testTerminateProcessOnCoordinatorAfterApproveBeforeSendMessage();
#endif
    BaseExchangePaymentTransaction::runThreeNodesCyclesTransactions();
    BaseExchangePaymentTransaction::runFourNodesCyclesTransactions();

    return resultOK();
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::reject(
    const char* message)
{
    BaseExchangePaymentTransaction::reject(message);
    // informAllNodesAboutTransactionFinish() will be added later
    return resultNoConsensusError();
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::propagateVotesListAndWaitForVotingResult()
{
    debug() << "propagateVotesListAndWaitForVotingResult. Total participants included: "
            << mParticipantsPublicKeys.size();
#ifdef DEBUG
    debug() << "Participants order is the next:";
    for (const auto &paymentNodeIdAndContractor : mPaymentParticipants) {
        debug() << paymentNodeIdAndContractor.first << " " << paymentNodeIdAndContractor.second->mainAddress()->fullAddress();
    }
#endif

#ifdef TESTS
    mSubsystemsController->testForbidSendMessageOnVoteStage();
#endif

    // send message with all public keys to all participants and wait for voting results
    for (const auto &paymentNodeIdAndAddress : mPaymentParticipants) {
        if (paymentNodeIdAndAddress.first == kCoordinatorPaymentNodeID) {
            continue;
        }
        sendMessage<ParticipantsPublicKeysMessage>(
            paymentNodeIdAndAddress.second->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            mParticipantsPublicKeys);
    }

    // TODO: additional check if payment is correct

    mParticipantsSignatures.clear();

#ifdef TESTS
    mSubsystemsController->testThrowExceptionOnVoteStage();
    mSubsystemsController->testTerminateProcessOnVoteStage();
#endif

    mStep = Stages::Common_VotesChecking;
    return resultWaitForMessageTypes( {
        Message::Payments_ParticipantVote,
        Message::Payments_TTLProlongationRequest},
    maxNetworkDelay(6));
}

void CoordinatorExchangePaymentTransaction::addFinalConfigurationOnPath(
    const PathID &pathID,
    OptimalPathResult *pathStats)
{
    debug() << "Add final configuration on path " << pathID;

    // Step 1: Validate flows vector
    if (pathStats->flows.empty()) {
        throw ValueError(
            "CoordinatorExchangePaymentTransaction::addFinalConfigurationOnPath: "
            "flows vector is empty - calculateFlows() must be called first");
    }

    size_t expectedFlowsSize = pathStats->mIntermediateNodesStates.size() + 1;
    if (pathStats->flows.size() != expectedFlowsSize) {
        throw ValueError(
            "CoordinatorExchangePaymentTransaction::addFinalConfigurationOnPath: "
            "flows vector size mismatch: expected " + to_string(expectedFlowsSize) +
            ", got " + to_string(pathStats->flows.size()));
    }

    // Step 2: Add payment participants (unchanged from original)
    for (const auto &contractor : mCurrentPathParticipants) {
        bool participantIncluded = false;
        for (const auto &paymentParticipant : mPaymentParticipants) {
            if (contractor == paymentParticipant.second) {
                participantIncluded = true;
                break;
            }
        }
        if (!participantIncluded) {
            mPaymentParticipants.insert(
                make_pair(
                    mCurrentFreePaymentID,
                    contractor));
            mPaymentNodesIds.insert(
                make_pair(
                    contractor->mainAddress()->fullAddress(),
                    mCurrentFreePaymentID));
            mCurrentFreePaymentID++;
        }
    }

    // Step 3: Add configurations for intermediate nodes
    // Each intermediate node gets TWO reservations: incoming and outgoing
    for (const auto &contractor : mCurrentPathParticipants) {
        int position = pathStats->path().positionOfNode(contractor->mainAddress());
        if (position < 0) {
            throw ValueError(
                "CoordinatorExchangePaymentTransaction::addFinalConfigurationOnPath: "
                "Intermediate node not found in path: " +
                contractor->mainAddress()->fullAddress());
        }

        auto nodeKey = contractor->mainAddress()->fullAddress();
        debug() << "nodeKey: " << nodeKey << " position: " << position;

        // Add incoming reservation (from previous node)
        //if (position > 0) {
            const auto& incomingFlow = pathStats->flows[position];
            PathReservation incomingReservation(
                pathID,
                make_shared<const TrustLineAmount>(incomingFlow.first),
                incomingFlow.second,
                PathReservation::Incoming);
            debug() << "incoming reservation for node: " << contractor->mainAddress()->fullAddress()
                    << " amount: " << *incomingReservation.amount
                    << " equivalent: " << incomingReservation.equivalent;

            if (mNodesFinalAmountsConfiguration.find(nodeKey) ==
                mNodesFinalAmountsConfiguration.end()) {
                mNodesFinalAmountsConfiguration[nodeKey] = {incomingReservation};
            } else {
                mNodesFinalAmountsConfiguration[nodeKey].push_back(incomingReservation);
            }
        //}

        // Add outgoing reservation (to next node)
        if (position < static_cast<int>(pathStats->path().nodes.size()) - 1) {
            const auto& outgoingFlow = pathStats->flows[position + 1];
            PathReservation outgoingReservation(
                pathID,
                make_shared<const TrustLineAmount>(outgoingFlow.first),
                outgoingFlow.second,
                PathReservation::Outgoing);
            debug() << "outgoing reservation for node: " << contractor->mainAddress()->fullAddress()
                << " amount: " << *outgoingReservation.amount
                << " equivalent: " << outgoingReservation.equivalent;

            mNodesFinalAmountsConfiguration[nodeKey].push_back(outgoingReservation);
        }
    }

    // Step 4: Add incoming reservation for receiver
    int receiverPosition = pathStats->path().positionOfNode(mContractor->mainAddress());
    if (receiverPosition < 0) {
        throw ValueError(
            "CoordinatorExchangePaymentTransaction::addFinalConfigurationOnPath: "
            "Receiver not found in path: " +
            mContractor->mainAddress()->fullAddress());
    }

    const auto& receiverIncomingFlow = pathStats->flows[receiverPosition];
    PathReservation receiverReservation(
        pathID,
        make_shared<const TrustLineAmount>(receiverIncomingFlow.first),
        receiverIncomingFlow.second,
        PathReservation::Incoming);
    debug() << "receiver incoming reservation for node: " << mContractor->mainAddress()->fullAddress()
            << " amount: " << *receiverReservation.amount
            << " equivalent: " << receiverReservation.equivalent;

    auto receiverKey = mContractor->mainAddress()->fullAddress();
    if (mNodesFinalAmountsConfiguration.find(receiverKey) ==
        mNodesFinalAmountsConfiguration.end()) {
        mNodesFinalAmountsConfiguration[receiverKey] = {receiverReservation};
    } else {
        mNodesFinalAmountsConfiguration[receiverKey].push_back(receiverReservation);
    }

    commitPathCommissions(pathID);
}

void CoordinatorExchangePaymentTransaction::sendFinalPathConfiguration(
    OptimalPathResult *pathStats,
    const PathID &pathID)
{
    debug() << "sendFinalPathConfiguration";

    const auto senderEquivalent = pathStats->currentPathEquivalent();

#ifdef TESTS
    mSubsystemsController->testForbidSendMessageWithFinalPathConfiguration(
        (uint32_t)pathStats->path().intermediates().size() - 1);
#endif
    for (const auto &intermediateNode : pathStats->path().intermediates()) {
        if (intermediateNode == mContractor->mainAddress()) {
            continue;
        }

        auto nodeKey = intermediateNode->fullAddress();
        debug() << "send message with final path configuration for node " << nodeKey;

        // Find reservations for this node
        auto nodeConfigIter = mNodesFinalAmountsConfiguration.find(nodeKey);
        if (nodeConfigIter == mNodesFinalAmountsConfiguration.end()) {
            warning() << "No configuration found for intermediate node " << nodeKey;
            continue;
        }

        // Find incoming and outgoing reservations for this pathID
        TrustLineAmount incomingAmount = TrustLineAmount(0);
        SerializedEquivalent incomingEquivalent = senderEquivalent;
        TrustLineAmount outgoingAmount = TrustLineAmount(0);
        SerializedEquivalent outgoingEquivalent = senderEquivalent;

        bool foundIncoming = false;
        bool foundOutgoing = false;

        for (const auto &reservation : nodeConfigIter->second) {
            if (reservation.pathID == pathID) {
                if (reservation.direction == PathReservation::Incoming) {
                    incomingAmount = *reservation.amount;
                    incomingEquivalent = reservation.equivalent;
                    foundIncoming = true;
                } else if (reservation.direction == PathReservation::Outgoing) {
                    outgoingAmount = *reservation.amount;
                    outgoingEquivalent = reservation.equivalent;
                    foundOutgoing = true;
                }
            }
        }

        if (!foundIncoming || !foundOutgoing) {
            warning() << "Incomplete reservations for node " << nodeKey
                      << " pathID " << pathID
                      << " (incoming: " << foundIncoming
                      << ", outgoing: " << foundOutgoing << ")";
            continue;  // Skip this node - cannot send valid configuration
        }

        debug() << "Sending final path configuration: pathID=" << pathID
                << " incoming=" << incomingAmount << " (equiv " << incomingEquivalent << ")"
                << " outgoing=" << outgoingAmount << " (equiv " << outgoingEquivalent << ")";

        sendMessage<FinalPathExchangeConfigurationMessage>(
            intermediateNode,
            senderEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            pathID,
            incomingAmount,
            incomingEquivalent,
            outgoingAmount,
            outgoingEquivalent);
    }
}

void CoordinatorExchangePaymentTransaction::prepareCommissionsForPath(
    OptimalPathResult *pathStats,
    const PathID &pathID)
{
    if (pathStats == nullptr) {
        return;
    }

    auto pendingIt = mPathPendingCommissions.find(pathID);
    if (pendingIt == mPathPendingCommissions.end()) {
        pendingIt = mPathPendingCommissions.emplace(pathID, set<CommissionKey>{}).first;
    }

    auto &pendingSet = pendingIt->second;
    pendingSet.clear();

    bool commissionAdjusted = false;

    auto &exchangeSteps = pathStats->path().exchangeSteps;
    for (auto &step : exchangeSteps) {
        if (step.fromEquivalent != step.toEquivalent) {
            continue;
        }

        if (step.commission == TrustLineAmount(0)) {
            continue;
        }

        CommissionKey key{step.nodeID, step.fromEquivalent};
        if (mConsumedCommissions.find(key) != mConsumedCommissions.end()) {
            debug() << "Commission already consumed for node " << step.nodeID
                    << ", eq=" << step.fromEquivalent
                    << " on path " << pathID << "; skipping deduction";
            step.commission = TrustLineAmount(0);
            commissionAdjusted = true;
        } else {
            pendingSet.insert(key);
        }
    }

    if (commissionAdjusted) {
        try {
            pathStats->calculateFlows(pathStats->paymentFlow);
        } catch (const std::exception &e) {
            warning() << "Failed to recalculate flows for path " << pathID
                      << " after commission adjustment: " << e.what();
        }
    }

    if (!pathStats->flows.empty()) {
        pathStats->received_amount = pathStats->flows.back().first;
    }

    if (pendingSet.empty()) {
        mPathPendingCommissions.erase(pendingIt);
    }
}

void CoordinatorExchangePaymentTransaction::commitPathCommissions(const PathID &pathID)
{
    auto pendingIt = mPathPendingCommissions.find(pathID);
    if (pendingIt == mPathPendingCommissions.end()) {
        return;
    }

    auto &pendingSet = pendingIt->second;
    if (pendingSet.empty()) {
        mPathPendingCommissions.erase(pendingIt);
        return;
    }

    auto &committedSet = mPathCommittedCommissions[pathID];
    for (const auto &key : pendingSet) {
        if (mConsumedCommissions.insert(key).second) {
            debug() << "Marking commission as consumed for node " << key.first
                    << ", eq=" << key.second
                    << " (path " << pathID << ")";
        }
        committedSet.insert(key);
    }

    mPathPendingCommissions.erase(pendingIt);
}

void CoordinatorExchangePaymentTransaction::releasePathCommissions(
    const PathID &pathID,
    bool releaseCommitted)
{
    mPathPendingCommissions.erase(pathID);

    if (!releaseCommitted) {
        return;
    }

    auto committedIt = mPathCommittedCommissions.find(pathID);
    if (committedIt == mPathCommittedCommissions.end()) {
        return;
    }

    for (const auto &key : committedIt->second) {
        auto consumedIt = mConsumedCommissions.find(key);
        if (consumedIt != mConsumedCommissions.end()) {
            debug() << "Releasing consumed commission for node " << key.first
                    << ", eq=" << key.second
                    << " due to path " << pathID << " rollback";
            mConsumedCommissions.erase(consumedIt);
        }
    }

    mPathCommittedCommissions.erase(committedIt);
}

TrustLineAmount CoordinatorExchangePaymentTransaction::calculateTotalReservedAmount() const
{
    // Calculate total amount already successfully reserved across all processed paths
    // Only count paths where reservation is actually approved (not just valid/added)
    TrustLineAmount total = TrustLineAmount(0);

    for (const auto &[pathID, pathStats] : mPathsStats) {
        // Check if path has been successfully reserved
        bool isReserved = false;

        if (pathStats->containsIntermediateNodes()) {
            // Path with intermediate nodes: check if last intermediate node approved
            isReserved = pathStats->isLastIntermediateNodeApproved();
        } else {
            // Direct path to receiver: check if receiver approved
            // For direct paths, mIntermediateNodesStates is empty, so we check differently
            // In this case, we consider path reserved when maxFlow > 0 and path is valid
            // (reservation would have been confirmed during processing)
            isReserved = (pathStats->optimal_flow > TrustLineAmount(0));
        }

        if (isReserved) {
            try {
                total = total + pathStats->received_amount;
            } catch (const std::exception &e) {
                warning() << "Error adding path received_amount: " << e.what()
                          << " for pathID=" << pathID;
                continue;
            }
        }
    }

    debug() << "Total reserved amount: " << total;
    return total;
}

/**
 * Aggregates the receive-side capacity that still can be used after the currently processed path.
 *
 * The helper is invoked after path rebuilding to decide if continuing with reservations makes
 * sense. It performs the following steps:
 *  1. Determines how much of the target amount (`mAmount`) is still unfulfilled.
 *  2. Iterates over `mPathsStats` and considers only paths with identifiers greater than the
 *     current reservation path (`mCurrentAmountReservingPathIdentifier`).
 *  3. Skips paths that were invalidated or already had their last intermediate node processed.
 *  4. Sums `received_amount` for each eligible path with detailed debug logging, guarding against
 *     arithmetic overflow/underflow exceptions thrown by `TrustLineAmount`.
 *  5. Logs a final info-level summary containing both the remaining needed amount and the total
 *     capacity identified by the helper.
 */
TrustLineAmount CoordinatorExchangePaymentTransaction::calculateTotalPathCapacityForReceive() const
{
    const TrustLineAmount totalReserved = calculateTotalReservedAmount();
    TrustLineAmount remainingNeeded = TrustLineAmount(0);

    if (totalReserved < mAmount) {
        remainingNeeded = mAmount - totalReserved;
    } else {
        info() << "Capacity helper: target already satisfied (reserved " << totalReserved
                << ") - remainingNeeded set to 0";
    }

    TrustLineAmount totalCapacity = TrustLineAmount(0);

    for (const auto &[pathID, pathStatsPtr] : mPathsStats) {
        if (pathID <= mCurrentAmountReservingPathIdentifier) {
            debug() << "Capacity helper: skip path " << pathID
                    << " (processed pathID <= current "
                    << mCurrentAmountReservingPathIdentifier << ")";
            continue;
        }

        if (pathStatsPtr == nullptr) {
            warning() << "Capacity helper: missing path stats for pathID " << pathID;
            continue;
        }

        const OptimalPathResult *pathStats = pathStatsPtr.get();

        if (!pathStats->isValid()) {
            debug() << "Capacity helper: skip path " << pathID << " because it is invalid";
            continue;
        }

        if (pathStats->isLastIntermediateNodeProcessed()) {
            debug() << "Capacity helper: skip path " << pathID
                    << " because its last intermediate node is already processed";
            continue;
        }

        const TrustLineAmount contribution = pathStats->received_amount;
        debug() << "Capacity helper: path " << pathID
                << " contributes receive capacity " << contribution;

        try {
            totalCapacity = totalCapacity + contribution;
        } catch (const std::exception &e) {
            warning() << "Capacity helper: failed to accumulate contribution from path "
                      << pathID << ": " << e.what();
        }
    }

    info() << "Capacity helper: aggregated receive capacity " << totalCapacity
            << " for remaining " << remainingNeeded
            << " (current pathID " << mCurrentAmountReservingPathIdentifier << ")";

    return totalCapacity;
}

TrustLineAmount CoordinatorExchangePaymentTransaction::calculateTotalReservedPaymentAmount() const
{
    TrustLineAmount totalReserved = TrustLineAmount(0);

    // Iterate through all exchange equivalents
    for (const auto equivalent : mCommand->exchangeEquivalents()) {
        // Get reserved amount for this equivalent using existing API
        const auto reservedForEquivalent = totalReservedAmount(
            AmountReservation::Outgoing,
            equivalent);

        // Add to total
        totalReserved = totalReserved + reservedForEquivalent;
    }

    return totalReserved;
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::proceedToNextStage()
{
    // Transition to next transaction stage after sufficient capacity reserved
    info() << "Sufficient capacity reserved, proceeding to final amounts configuration";

    // Request observing block number resource before proceeding
    mResourcesManager->requestObservingBlockNumber(mTransactionUUID);

    // Transition to final amounts configuration stage (via observing block number)
    mStep = Stages::Common_ObservingBlockNumberProcessing;

    return resultWaitForResourceTypes(
        {BaseResource::ObservingBlockNumber},
        maxNetworkDelay(1));
}

bool CoordinatorExchangePaymentTransaction::validatePathForProcessing(
    const OptimalPathResult *pathStats)
{
    const auto &path = pathStats->path();

    // Check for empty path.nodes
    // Though this should be prevented by addPathForFurtherProcessing (task 08-03),
    // we still validate as an edge case safety check
    if (path.nodes.empty()) {
        error() << "Path filtering: path.nodes is empty, skipping path";
        return false;
    }

    // Check for inaccessible nodes
    // Note: Use path.nodes (BaseAddress::Shared) for filtering
    // path.nodes is populated by addPathForFurtherProcessing and guaranteed to be available here
    for (const auto &nodeAddress : path.nodes) {
        if (std::find(mInaccessibleNodes.begin(),
                      mInaccessibleNodes.end(),
                      nodeAddress) != mInaccessibleNodes.end()) {
            info() << "Path filtering: contains inaccessible node: "
                   << nodeAddress->fullAddress();
            return false;
        }
    }

    // Check for rejected trust lines
    // Iterate through edges in path.nodes (consecutive pairs)
    for (size_t i = 0; i + 1 < path.nodes.size(); ++i) {
        auto source = path.nodes[i];
        auto dest = path.nodes[i + 1];

        for (const auto &[rejSource, rejDest] : mRejectedTrustLines) {
            if (source == rejSource && dest == rejDest) {
                info() << "Path filtering: contains rejected trust line: "
                       << source->fullAddress() << " -> "
                       << dest->fullAddress();
                return false;
            }
        }
    }

    return true;  // Path is valid
}

bool CoordinatorExchangePaymentTransaction::exceedsAllowablePaymentAmount(
    const TrustLineAmount &amount) const
{
    return amount > mCommand->maxAllowablePaymentAmount();
}

/**
 * Handles condition changes (exchange rate or commission mismatch) detected during payment path reservation.
 *
 * This method is invoked when an intermediate node reports different exchange rate or commission values
 * than what the coordinator expected based on cached information. The method performs several operations:
 *
 * 1. Identifies the affected node and its position(s) in the path
 * 2. Updates the appropriate caches:
 *    - ExchangeRatesManager for exchange rates (mExternalExchangeRates)
 *    - TopologyTrustLinesManager for commissions (mCommissionsCache)
 * 3. Drops all reservations on the affected path
 * 4. Marks the original path as unusable
 * 5. Creates a new path with updated conditions and recalculated flows
 * 6. Inserts the new path into mPathsStats and mPathIDs for processing
 *
 * Path structure considerations:
 * - path.ids may contain duplicate node IDs for in-place exchanges (e.g., [0, 5, 5, 3] where node 5 exchanges)
 * - path.nodes contains unique node addresses (deduplicated)
 * - For exchange nodes: first occurrence is incoming side, second is outgoing side
 * - Commission is charged after exchange (at the second occurrence if node is exchanger)
 *
 * @param pathID The identifier of the payment path where condition change was detected
 * @param actualExchangeRate The actual exchange rate reported by node (mantissa, exponent).
 *                          If nullopt: rate is no longer available, remove from cache and skip path reconstruction
 *                          If has_value: update cache and use this rate for new path
 * @param actualCommission The actual commission reported by node.
 *                        If nullopt: commission not provided (not changed, skip commission update)
 *                        If zero: remove commission from cache
 *                        If non-zero: update cache with this commission value
 * @return TransactionResult indicating whether to continue with next path or terminate transaction
 */
TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::handleConditionChange(
    const PathID &pathID,
    const optional<pair<TrustLineAmount, int16_t>> &actualExchangeRate,
    const optional<TrustLineAmount> &actualCommission)
{
    debug() << "handleConditionChange for pathID=" << pathID;

    // Step 1: Get path stats
    auto pathStatsIt = mPathsStats.find(pathID);
    if (pathStatsIt == mPathsStats.end()) {
        warning() << "Path not found for pathID=" << pathID;
        return tryProcessNextPath();
    }

    OptimalPathResult *pathStats = pathStatsIt->second.get();

    // Determine which node triggered the rejection
    // affectedNodePosition is index in mPath.intermediates() (intermediate nodes vector)
    auto nodeAndPos = pathStats->currentIntermediateNodeAndPos();
    SerializedPositionInPath intermediateNodeIndex = nodeAndPos.second;
    BaseAddress::Shared affectedNodeAddress = nodeAndPos.first;

    const auto &path = pathStats->path();

    // Get affected node ID by searching path.ids for the node that matches affectedNodeAddress
    // We can't use path.ids[intermediateNodeIndex + 1] because path.nodes is deduplicated
    // while path.ids may have duplicates for in-place exchanges
    ContractorID affectedNodeID = 0;
    for (SerializedPositionInPath idx = 1; idx < path.ids.size(); ++idx) {
        ContractorID nodeID = path.ids[idx];
        if (nodeID == 0) continue;  // Skip sender

        auto nodeAddress = mEquivalentsSubsystemsRouter->getParticipantAddress(nodeID);
        if (nodeAddress && nodeAddress->fullAddress() == affectedNodeAddress->fullAddress()) {
            affectedNodeID = nodeID;
            break;
        }
    }

    if (affectedNodeID == 0) {
        error() << "Could not find affected node ID for address " << affectedNodeAddress->fullAddress();
        return tryProcessNextPath();
    }

    debug() << "Affected intermediate node index=" << static_cast<int>(intermediateNodeIndex)
            << ", address=" << affectedNodeAddress->fullAddress()
            << ", nodeID=" << affectedNodeID;

    // Find the position(s) in path.ids/equivalents where this node performs exchange/commission
    // path.ids may have duplicates for in-place exchanges (same nodeID appears twice)
    SerializedPositionInPath affectedPositionInPath = 0;
    SerializedPositionInPath exchangePositionInPath = 0;
    SerializedPositionInPath commissionPositionInPath = 0;
    bool foundPosition = false;
    bool foundExchangePosition = false;
    bool foundCommissionPosition = false;

    // Search for the node in path.ids considering it might be duplicated for exchange
    for (SerializedPositionInPath idx = 0; idx < path.ids.size(); ++idx) {
        if (path.ids[idx] == affectedNodeID) {
            // Check if this is an exchange position (same ID at idx and idx+1)
            if (idx + 1 < path.ids.size() && path.ids[idx + 1] == affectedNodeID) {
                // This is an in-place exchange at positions [idx, idx+1]
                // Always set exchange position when node appears twice (even if rate is nullopt - for deletion)
                exchangePositionInPath = idx;
                affectedPositionInPath = idx;
                foundExchangePosition = true;
                foundPosition = true;

                // If node also has commission, it's charged AFTER the exchange (at idx+1)
                if (actualCommission) {
                    commissionPositionInPath = idx + 1;
                    affectedPositionInPath = idx + 1;  // For commission storage use outgoing equivalent
                    foundCommissionPosition = true;
                }
                break;  // Found duplicate - this is the exchanger
            }
            // This position is for commission only (node appears once, no exchange)
            if (actualCommission && !foundCommissionPosition) {
                commissionPositionInPath = idx;
                affectedPositionInPath = idx;
                foundCommissionPosition = true;
                foundPosition = true;
            }
        }
    }

    if (!foundPosition) {
        error() << "Could not find affected node position in path.ids";
        return tryProcessNextPath();
    }

    debug() << "Affected position in path.ids=" << static_cast<int>(affectedPositionInPath)
            << ", exchangePosition=" << static_cast<int>(exchangePositionInPath)
            << ", commissionPosition=" << static_cast<int>(commissionPositionInPath);

    // Step 2: Update ExchangeRatesManager.mExternalExchangeRates and TopologyTrustLinesManager.mCommissionsCache (R5, R6)
    if (foundExchangePosition && exchangePositionInPath + 1 < path.equivalents.size()) {
        SerializedEquivalent incomingEquiv = path.equivalents[exchangePositionInPath];
        SerializedEquivalent outgoingEquiv = path.equivalents[exchangePositionInPath + 1];

        if (actualExchangeRate) {
            // Update ExchangeRatesManager.mExternalExchangeRates (R5)
            // Store as external rate from affected node, don't update TTL
            try {
                // Create ExchangeRate object with current timestamp as expiry (won't be used since we don't update TTL)
                auto now = utc_now();
                auto expiryTime = now + boost::posix_time::milliseconds(300000);  // 5 minutes TTL
                ExchangeRate rate(
                    incomingEquiv,
                    outgoingEquiv,
                    actualExchangeRate->first,
                    actualExchangeRate->second,
                    expiryTime,
                    TrustLineAmount(0),  // minExchangeAmount - not provided in response
                    TrustLineAmount(0)); // maxExchangeAmount - not provided in response

                mExchangeRatesManager->addOrUpdateExternal(
                    affectedNodeID,
                    rate);

                info() << "Stored external exchange rate from contractor " << affectedNodeID
                       << ": " << incomingEquiv << "->" << outgoingEquiv
                       << " = " << actualExchangeRate->first << " * 10^" << actualExchangeRate->second;
            } catch (const exception &e) {
                error() << "Failed to store external exchange rate: " << e.what();
            }
        } else {
            // Exchange rate is no longer available - remove from cache
            try {
                mExchangeRatesManager->removeExternal(
                    affectedNodeID,
                    incomingEquiv,
                    outgoingEquiv);

                info() << "Removed external exchange rate from contractor " << affectedNodeID
                       << " (" << incomingEquiv << "->" << outgoingEquiv << ") - rate no longer available";
            } catch (const exception &e) {
                error() << "Failed to remove external exchange rate: " << e.what();
            }
        }
    }

    if (actualCommission && foundCommissionPosition &&
        commissionPositionInPath < path.equivalents.size()) {
        // Commission is on the equivalent at the commission position (after exchange if node is exchanger)
        SerializedEquivalent equivalent = path.equivalents[commissionPositionInPath];

        // Update TopologyTrustLinesManager.mCommissionsCache via EquivalentsSubsystemsRouter (R6)
        // Don't update TTL
        try {
            auto topologyTrustLineManager = mEquivalentsSubsystemsRouter->topologyTrustLineManager(equivalent);
            if (!topologyTrustLineManager) {
                error() << "TopologyTrustLinesManager not found for equivalent " << equivalent;
            } else {
                if (*actualCommission > TrustLineAmount(0)) {
                    // Create Commission object
                    uint64_t commissionValue = static_cast<uint64_t>(*actualCommission);
                    auto commissionShared = make_shared<Commission>(commissionValue);

                    topologyTrustLineManager->storeCommission(
                        affectedNodeID,
                        equivalent,
                        commissionShared);

                    info() << "Stored commission in cache for contractor " << affectedNodeID
                           << ", eq=" << equivalent
                           << ", commission=" << *actualCommission;
                } else {
                    // Zero commission received - remove existing commission from cache
                    topologyTrustLineManager->removeCommission(affectedNodeID, equivalent);
                    info() << "Removed commission from cache for contractor " << affectedNodeID
                           << ", eq=" << equivalent << " (zero commission received)";
                }
            }
        } catch (const exception &e) {
            error() << "Failed to store commission in cache: " << e.what();
        }
    }

    // Step 3: Save critical path parameters before they are reset
    // IMPORTANT: Save mMaxPathFlow and mMaxPathReceivedAmount BEFORE dropReservationsOnPath
    // because setUnusable() will reset mMaxPathFlow to 0
    TrustLineAmount savedMaxPathFlow = pathStats->mMaxPathFlow;
    TrustLineAmount savedMaxPathReceivedAmount = pathStats->mMaxPathReceivedAmount;
    TrustLineAmount savedPaymentFlow = pathStats->paymentFlow;

    debug() << "Saved path parameters: mMaxPathFlow=" << savedMaxPathFlow
            << ", mMaxPathReceivedAmount=" << savedMaxPathReceivedAmount
            << ", paymentFlow=" << savedPaymentFlow;

    // Step 4: Drop reservations on this path
    debug() << "Dropping reservations on invalidated path " << pathID;
    dropReservationsOnPath(pathStats, pathID, /* sendToLastProcessedNode */ false);

    // Step 5: Mark path unusable
    pathStats->setUnusable();
    info() << "Marked path " << pathID << " as unusable due to condition change";

    // Step 4.5: Check if path can be reconstructed
    // If exchange rate is no longer available (nullopt) for an exchange node, we cannot create a new path
    if (foundExchangePosition && !actualExchangeRate) {
        error() << "Cannot create new path: exchange rate no longer available for contractor "
                << affectedNodeID;
        // Try to process next path instead of creating impossible path
        return tryProcessNextPath();
    }

    // Step 5: Update or add exchangeSteps in path with actual values
    ExchangePath updatedPath = path;  // Copy original path

    bool foundExchangeStep = false;
    bool foundCommissionStep = false;

    // First, try to update existing exchangeSteps
    for (auto &step : updatedPath.exchangeSteps) {
        // Check if this is the affected node's exchange step
        if (step.nodeID == affectedNodeID) {
            // Update exchange rate if this step matches
            if (actualExchangeRate && foundExchangePosition &&
                exchangePositionInPath + 1 < path.equivalents.size() &&
                step.fromEquivalent == path.equivalents[exchangePositionInPath] &&
                step.toEquivalent == path.equivalents[exchangePositionInPath + 1]) {

                step.exchangeRate = actualExchangeRate->first;
                step.exchangeRateShift = actualExchangeRate->second;
                foundExchangeStep = true;
                debug() << "Updated exchangeStep: nodeID=" << step.nodeID
                        << ", rate=" << step.exchangeRate
                        << ", shift=" << step.exchangeRateShift;
            }

            // Update commission if this step matches (same equiv on both sides)
            if (actualCommission && foundCommissionPosition &&
                step.fromEquivalent == step.toEquivalent &&
                step.fromEquivalent == path.equivalents[commissionPositionInPath]) {

                step.commission = *actualCommission;
                foundCommissionStep = true;
                debug() << "Updated exchangeStep: nodeID=" << step.nodeID
                        << ", commission=" << step.commission;
            }
        }
    }

    // If commission was not found in existing steps, add a new exchangeStep for it
    if (actualCommission && foundCommissionPosition && !foundCommissionStep) {
        SerializedEquivalent equivalent = path.equivalents[commissionPositionInPath];

        ExchangeStep newCommissionStep;
        newCommissionStep.nodeID = affectedNodeID;
        newCommissionStep.fromEquivalent = equivalent;
        newCommissionStep.toEquivalent = equivalent;
        newCommissionStep.exchangeRate = 0;
        newCommissionStep.exchangeRateShift = 0;
        newCommissionStep.commission = *actualCommission;
        newCommissionStep.minExchangeAmount = 0;
        newCommissionStep.maxExchangeAmount = 0;

        updatedPath.exchangeSteps.push_back(newCommissionStep);
        debug() << "Added new commission exchangeStep: nodeID=" << affectedNodeID
                << ", equiv=" << equivalent
                << ", commission=" << *actualCommission;
    }

    // Step 6: Calculate new optimal flow and received amount with updated conditions
    // Strategy: Try to preserve maximum receiver-side capacity (mMaxPathReceivedAmount) by
    // recalculating required sender-side flow. If new flow exceeds sender capacity (mMaxPathFlow),
    // fall back to preserving sender capacity and recalculating receiver amount.
    //
    // Use SAVED values because pathStats fields were reset by dropReservationsOnPath/setUnusable

    debug() << "Starting recalculation with saved values: mMaxPathFlow=" << savedMaxPathFlow
            << ", mMaxPathReceivedAmount=" << savedMaxPathReceivedAmount
            << ", paymentFlow=" << savedPaymentFlow;

    // Step 6.1: Calculate required optimal_flow to deliver mMaxPathReceivedAmount with updated conditions
    TrustLineAmount newOptimalFlow;
    try {
        newOptimalFlow = pathStats->calculateOptimalFlowWithUpdatedConditions(
            savedMaxPathReceivedAmount,
            affectedPositionInPath,
            actualExchangeRate,
            actualCommission);

        debug() << "Calculated new optimal_flow=" << newOptimalFlow
                << " to deliver mMaxPathReceivedAmount=" << savedMaxPathReceivedAmount;
    } catch (const exception &e) {
        error() << "Error calculating optimal flow with updated conditions: " << e.what();
        return tryProcessNextPath();
    }

    // Step 6.2: Determine final flow and received amount based on sender capacity constraint
    TrustLineAmount finalOptimalFlow;
    TrustLineAmount finalReceivedAmount;

    if (newOptimalFlow <= savedMaxPathFlow) {
        // New conditions are acceptable: required flow fits within sender capacity.
        // Use the full receiver capacity (mMaxPathReceivedAmount) as target.
        finalOptimalFlow = newOptimalFlow;
        finalReceivedAmount = savedMaxPathReceivedAmount;

        info() << "New optimal_flow (" << newOptimalFlow << ") <= mMaxPathFlow (" << savedMaxPathFlow
               << "): using full receiver capacity (" << savedMaxPathReceivedAmount << ")";
    } else {
        // New conditions require more sender resources than available.
        // Constrain by sender capacity (mMaxPathFlow) and recalculate receiver amount.
        finalOptimalFlow = savedMaxPathFlow;

        try {
            finalReceivedAmount = pathStats->calculateReceivedAmountWithUpdatedConditions(
                savedMaxPathFlow,
                affectedPositionInPath,
                actualExchangeRate,
                actualCommission);

            info() << "New optimal_flow (" << newOptimalFlow << ") > mMaxPathFlow (" << savedMaxPathFlow
                   << "): constraining to mMaxPathFlow, receivedAmount=" << finalReceivedAmount;
        } catch (const exception &e) {
            error() << "Error calculating received amount with sender capacity constraint: " << e.what();
            return tryProcessNextPath();
        }
    }

    // Step 7: Create new OptimalPathResult with properly copied state
    auto newPathStats = make_unique<OptimalPathResult>();

    // Copy all fields from old path
    newPathStats->mPath = updatedPath;  // Use updated path with new exchangeSteps
    newPathStats->optimal_flow = finalOptimalFlow;

    // Set maximum flow constraints for the new path using SAVED values.
    // mMaxPathFlow: preserve the original sender-side capacity (what coordinator can pay)
    // mMaxPathReceivedAmount: preserve the original receiver-side capacity (what receiver can obtain)
    // These values represent the upper bounds and are independent of current flow calculations.
    newPathStats->mMaxPathFlow = savedMaxPathFlow;
    newPathStats->mMaxPathReceivedAmount = savedMaxPathReceivedAmount;

    newPathStats->received_amount = finalReceivedAmount;
    newPathStats->effective_exchange_rate = pathStats->effective_exchange_rate;
    newPathStats->path_efficiency = pathStats->path_efficiency;
    newPathStats->mIsValid = true;  // New path is valid

    // Initialize intermediate nodes states for reservation attempts
    // Reset all nodes to "not sent" so reservation can start from beginning
    if (!newPathStats->mPath.nodes.empty()) {
        // Exclude sender (first) and receiver (last) - only intermediate nodes
        size_t intermediateNodesCount = newPathStats->mPath.nodes.size() > 1 ?
                                         newPathStats->mPath.nodes.size() - 1 : 0;
        newPathStats->mIntermediateNodesStates.resize(
            intermediateNodesCount,
            OptimalPathResult::ReservationRequestDoesntSent);

        debug() << "Initialized " << intermediateNodesCount << " intermediate node states for new path";
    }

    // Recalculate flows using the calculateFlows method with final optimal flow
    try {
        newPathStats->calculateFlows(finalOptimalFlow);
        debug() << "Calculated flows for new path with finalOptimalFlow=" << finalOptimalFlow;
    } catch (const exception &e) {
        error() << "Error calculating flows for new path: " << e.what();
        return tryProcessNextPath();
    }

    // Update received_amount to match the last flow (actual amount that reaches receiver)
    if (!newPathStats->flows.empty()) {
        newPathStats->received_amount = newPathStats->flows.back().first;
        debug() << "Updated received_amount to match last flow: " << newPathStats->received_amount;
    }

    // Step 8: Insert new path and update PathIDs
    // Find current path index in mPathIDs
    auto currentIt = std::find(mPathIDs.begin(), mPathIDs.end(), pathID);
    if (currentIt == mPathIDs.end()) {
        error() << "PathID " << pathID << " not found in mPathIDs";
        return tryProcessNextPath();
    }

    size_t currentIndex = std::distance(mPathIDs.begin(), currentIt);

    // Increment all subsequent PathIDs by 1
    // IMPORTANT: Process in reverse order to avoid overwriting IDs during shift
    // Example: [1, 2, 3] -> shift from end: 3->4, then 2->3 (not 2->3 first, which would lose original 3)
    if (currentIndex + 1 < mPathIDs.size()) {
        for (size_t idx = mPathIDs.size() - 1; idx > currentIndex; --idx) {
            PathID oldID = mPathIDs[idx];
            PathID newID = oldID + 1;

            // Move the path stats to the new ID
            auto nodeHandler = mPathsStats.extract(oldID);
            if (nodeHandler.empty()) {
                error() << "Failed to extract pathStats for PathID=" << oldID;
                continue;
            }
            nodeHandler.key() = newID;
            mPathsStats.insert(std::move(nodeHandler));

            auto pendingHandler = mPathPendingCommissions.extract(oldID);
            if (!pendingHandler.empty()) {
                pendingHandler.key() = newID;
                mPathPendingCommissions.insert(std::move(pendingHandler));
            }

            auto committedHandler = mPathCommittedCommissions.extract(oldID);
            if (!committedHandler.empty()) {
                committedHandler.key() = newID;
                mPathCommittedCommissions.insert(std::move(committedHandler));
            }

            // Update the ID in the vector
            mPathIDs[idx] = newID;
        }
    }

    // Create new PathID (current + 1)
    PathID newPathID = pathID + 1;
    mPathsStats[newPathID] = std::move(newPathStats);

    prepareCommissionsForPath(mPathsStats[newPathID].get(), newPathID);

    // Insert new PathID into vector at position after current
    mPathIDs.insert(mPathIDs.begin() + currentIndex + 1, newPathID);

    info() << "Created new path " << newPathID
           << " with optimalFlow=" << finalOptimalFlow
           << ", receivedAmount=" << finalReceivedAmount;

    // Log detailed new path information for debugging
    auto *logPathStats = mPathsStats[newPathID].get();
    debug() << "=== New Path " << newPathID << " Details ===";
    debug() << "optimal_flow=" << logPathStats->optimal_flow;
    debug() << "received_amount=" << logPathStats->received_amount;
    debug() << "mMaxPathFlow=" << logPathStats->mMaxPathFlow;
    debug() << "mMaxPathReceivedAmount=" << logPathStats->mMaxPathReceivedAmount;
    debug() << "mIsValid=" << logPathStats->mIsValid;

    debug() << "flows.size()=" << logPathStats->flows.size();
    for (size_t i = 0; i < logPathStats->flows.size(); ++i) {
        debug() << "  flows[" << i << "]: amount=" << logPathStats->flows[i].first
                << ", equiv=" << logPathStats->flows[i].second;
    }

    debug() << "mIntermediateNodesStates.size()=" << logPathStats->mIntermediateNodesStates.size();
    for (size_t i = 0; i < logPathStats->mIntermediateNodesStates.size(); ++i) {
        debug() << "  mIntermediateNodesStates[" << i << "]=" << static_cast<int>(logPathStats->mIntermediateNodesStates[i]);
    }

    debug() << "mPath.ids.size()=" << logPathStats->mPath.ids.size();
    for (size_t i = 0; i < logPathStats->mPath.ids.size(); ++i) {
        debug() << "  mPath.ids[" << i << "]=" << logPathStats->mPath.ids[i];
    }

    debug() << "mPath.equivalents.size()=" << logPathStats->mPath.equivalents.size();
    for (size_t i = 0; i < logPathStats->mPath.equivalents.size(); ++i) {
        debug() << "  mPath.equivalents[" << i << "]=" << logPathStats->mPath.equivalents[i];
    }

    debug() << "mPath.nodes.size()=" << logPathStats->mPath.nodes.size();
    for (size_t i = 0; i < logPathStats->mPath.nodes.size(); ++i) {
        debug() << "  mPath.nodes[" << i << "]=" << logPathStats->mPath.nodes[i]->fullAddress();
    }

    debug() << "mPath.exchangeSteps.size()=" << logPathStats->mPath.exchangeSteps.size();
    for (size_t i = 0; i < logPathStats->mPath.exchangeSteps.size(); ++i) {
        const auto &step = logPathStats->mPath.exchangeSteps[i];
        debug() << "  exchangeStep[" << i << "]: nodeID=" << step.nodeID
                << ", fromEquiv=" << step.fromEquivalent
                << ", toEquiv=" << step.toEquivalent
                << ", rate=" << step.exchangeRate
                << ", shift=" << step.exchangeRateShift
                << ", commission=" << step.commission;
    }
    debug() << "=== End Path " << newPathID << " Details ===";

    // Step 9: Update all subsequent paths containing affected node (Task 09-04)
    // Note: Pass newPathID (not pathID) because paths have already been shifted
    updateSubsequentPathsWithChangedConditions(
        newPathID,
        affectedNodeAddress,
        actualExchangeRate,
        actualCommission);

    // Step 10: Continue processing
    return tryProcessNextPath();
}

bool CoordinatorExchangePaymentTransaction::pathContainsNode(
    OptimalPathResult *pathStats,
    BaseAddress::Shared targetNode) const
{
    const auto &path = pathStats->path();

    // Check all node addresses in path.nodes
    for (const auto &nodeAddress : path.nodes) {
        if (nodeAddress->fullAddress() == targetNode->fullAddress()) {
            return true;
        }
    }

    return false;
}

void CoordinatorExchangePaymentTransaction::updateSubsequentPathsWithChangedConditions(
    const PathID &pathID,
    BaseAddress::Shared affectedNodeAddress,
    const optional<pair<TrustLineAmount, int16_t>> &actualExchangeRate,
    const optional<TrustLineAmount> &actualCommission)
{
    debug() << "updateSubsequentPathsWithChangedConditions for pathID=" << pathID
            << ", affectedNode=" << affectedNodeAddress->fullAddress();

    // Step 1: Find current path index in mPathIDs
    auto currentIt = std::find(mPathIDs.begin(), mPathIDs.end(), pathID);
    if (currentIt == mPathIDs.end()) {
        warning() << "Current path not found in mPathIDs";
        return;
    }

    size_t currentIndex = std::distance(mPathIDs.begin(), currentIt);

    // Step 2: Iterate through all subsequent paths (starting from currentIndex + ,
    // because currentIndex is the newly created path)
    for (size_t idx = currentIndex + 1; idx < mPathIDs.size(); ++idx) {
        PathID subsequentPathID = mPathIDs[idx];
        auto pathIt = mPathsStats.find(subsequentPathID);

        if (pathIt == mPathsStats.end()) {
            warning() << "Path stats not found for subsequent pathID=" << subsequentPathID;
            continue;
        }
        debug() << "Path stats found for subsequent pathID=" << subsequentPathID;

        OptimalPathResult *pathStats = pathIt->second.get();

        // Step 3: Check if path contains affected node
        if (!pathContainsNode(pathStats, affectedNodeAddress)) {
            continue;  // Skip paths that don't involve this node
        }

        info() << "Updating subsequent path " << subsequentPathID
               << " affected by condition change on node " << affectedNodeAddress->fullAddress();

        const auto &path = pathStats->path();

        // Step 4: Find affected node ID and position in path
        ContractorID affectedNodeID = 0;
        for (SerializedPositionInPath pos = 1; pos < path.ids.size(); ++pos) {
            ContractorID nodeID = path.ids[pos];
            if (nodeID == 0) continue;  // Skip sender

            auto nodeAddress = mEquivalentsSubsystemsRouter->getParticipantAddress(nodeID);
            if (nodeAddress && nodeAddress->fullAddress() == affectedNodeAddress->fullAddress()) {
                affectedNodeID = nodeID;
                break;
            }
        }

        if (affectedNodeID == 0) {
            error() << "Could not find affected node ID in path " << subsequentPathID;
            continue;
        }

        // Step 5: Find the position in path.ids where conditions apply
        SerializedPositionInPath affectedPositionInPath = 0;
        bool foundPosition = false;
        bool isExchangePosition = false;

        // Search for the node in path.ids (may be duplicated for exchange)
        for (SerializedPositionInPath pos = 0; pos < path.ids.size(); ++pos) {
            if (path.ids[pos] == affectedNodeID) {
                // Check if this is an exchange position (same ID at pos and pos+1)
                if (pos + 1 < path.ids.size() && path.ids[pos + 1] == affectedNodeID) {
                    // In-place exchange at positions [pos, pos+1]
                    affectedPositionInPath = pos;
                    isExchangePosition = true;
                    foundPosition = true;

                    // For commission after exchange, use pos+1
                    if (actualCommission) {
                        affectedPositionInPath = pos + 1;
                    }
                    break;
                }
                // Commission only (node appears once)
                if (actualCommission && !foundPosition) {
                    affectedPositionInPath = pos;
                    foundPosition = true;
                }
            }
        }

        if (!foundPosition) {
            error() << "Could not find affected node position in path " << subsequentPathID;
            continue;
        }

        debug() << "Affected position in path.ids=" << static_cast<int>(affectedPositionInPath)
                << " for path " << subsequentPathID;

        // Step 6: Check if exchange rate no longer exists - mark path as invalid
        if (isExchangePosition && !actualExchangeRate) {
            warning() << "Exchange rate no longer available for path " << subsequentPathID
                      << " - marking path as invalid";
            pathStats->setUnusable();
            releasePathCommissions(subsequentPathID, true);
            continue;
        }

        // Step 7: Save critical parameters before recalculation
        TrustLineAmount savedMaxPathFlow = pathStats->mMaxPathFlow;
        TrustLineAmount savedMaxPathReceivedAmount = pathStats->mMaxPathReceivedAmount;
        TrustLineAmount oldReceivedAmount = pathStats->received_amount;

        debug() << "Path " << subsequentPathID << " before update: "
                << "mMaxPathFlow=" << savedMaxPathFlow
                << ", mMaxPathReceivedAmount=" << savedMaxPathReceivedAmount
                << ", optimal_flow=" << pathStats->optimal_flow
                << ", received_amount=" << oldReceivedAmount;

        // Step 7.5: Update exchangeSteps in path with actual values
        // This ensures subsequent paths use updated exchange rates and commissions
        ExchangePath &pathRef = pathStats->mPath;
        bool foundExchangeStep = false;
        bool foundCommissionStep = false;

        // Find position in path.equivalents for the affected node
        SerializedPositionInPath exchangePositionInPath = 0;
        SerializedPositionInPath commissionPositionInPath = 0;

        // Determine positions based on whether it's exchange or commission
        if (isExchangePosition) {
            // For exchange, affectedPositionInPath points to the first position
            exchangePositionInPath = affectedPositionInPath;
            if (actualCommission) {
                // Commission comes after exchange
                commissionPositionInPath = affectedPositionInPath + 1;
            }
        } else {
            // Commission only
            commissionPositionInPath = affectedPositionInPath;
        }

        // Update existing exchangeSteps
        for (auto &step : pathRef.exchangeSteps) {
            if (step.nodeID == affectedNodeID) {
                // Update exchange rate if this step matches
                if (actualExchangeRate && isExchangePosition &&
                    exchangePositionInPath + 1 < path.equivalents.size() &&
                    step.fromEquivalent == path.equivalents[exchangePositionInPath] &&
                    step.toEquivalent == path.equivalents[exchangePositionInPath + 1]) {

                    step.exchangeRate = actualExchangeRate->first;
                    step.exchangeRateShift = actualExchangeRate->second;
                    foundExchangeStep = true;
                    debug() << "Updated exchangeStep for path " << subsequentPathID
                            << ": nodeID=" << step.nodeID
                            << ", rate=" << step.exchangeRate
                            << ", shift=" << step.exchangeRateShift;
                }

                // Update commission if this step matches
                if (actualCommission &&
                    step.fromEquivalent == step.toEquivalent &&
                    step.fromEquivalent == path.equivalents[commissionPositionInPath]) {

                    step.commission = *actualCommission;
                    foundCommissionStep = true;
                    debug() << "Updated commission for path " << subsequentPathID
                            << ": nodeID=" << step.nodeID
                            << ", commission=" << step.commission;
                }
            }
        }

        // If commission was not found in existing steps, add a new exchangeStep for it
        if (actualCommission && !foundCommissionStep) {
            SerializedEquivalent equivalent = path.equivalents[commissionPositionInPath];

            ExchangeStep newCommissionStep;
            newCommissionStep.nodeID = affectedNodeID;
            newCommissionStep.fromEquivalent = equivalent;
            newCommissionStep.toEquivalent = equivalent;
            newCommissionStep.exchangeRate = 0;
            newCommissionStep.exchangeRateShift = 0;
            newCommissionStep.commission = *actualCommission;
            newCommissionStep.minExchangeAmount = 0;
            newCommissionStep.maxExchangeAmount = 0;

            pathRef.exchangeSteps.push_back(newCommissionStep);
            debug() << "Added new commission exchangeStep for path " << subsequentPathID
                    << ": nodeID=" << affectedNodeID
                    << ", equiv=" << equivalent
                    << ", commission=" << *actualCommission;
        }

        // Step 8: Calculate new optimal_flow to preserve mMaxPathReceivedAmount
        TrustLineAmount newOptimalFlow;
        try {
            newOptimalFlow = pathStats->calculateOptimalFlowWithUpdatedConditions(
                savedMaxPathReceivedAmount,
                affectedPositionInPath,
                actualExchangeRate,
                actualCommission);

            debug() << "Calculated new optimal_flow=" << newOptimalFlow
                    << " to deliver mMaxPathReceivedAmount=" << savedMaxPathReceivedAmount;
        } catch (const exception &e) {
            warning() << "Error calculating optimal flow for path " << subsequentPathID
                      << ": " << e.what();
            continue;
        }

        // Step 9: Determine final flow and received amount
        TrustLineAmount finalOptimalFlow;
        TrustLineAmount finalReceivedAmount;

        if (newOptimalFlow <= savedMaxPathFlow) {
            // New conditions acceptable: use full receiver capacity
            finalOptimalFlow = newOptimalFlow;
            finalReceivedAmount = savedMaxPathReceivedAmount;

            info() << "Path " << subsequentPathID << ": new optimal_flow (" << newOptimalFlow
                   << ") <= mMaxPathFlow (" << savedMaxPathFlow
                   << "), using full receiver capacity (" << savedMaxPathReceivedAmount << ")";
        } else {
            // New conditions require more resources: constrain by sender capacity
            finalOptimalFlow = savedMaxPathFlow;

            try {
                finalReceivedAmount = pathStats->calculateReceivedAmountWithUpdatedConditions(
                    savedMaxPathFlow,
                    affectedPositionInPath,
                    actualExchangeRate,
                    actualCommission);

                info() << "Path " << subsequentPathID << ": new optimal_flow (" << newOptimalFlow
                       << ") > mMaxPathFlow (" << savedMaxPathFlow
                       << "), constraining to mMaxPathFlow, receivedAmount=" << finalReceivedAmount;
            } catch (const exception &e) {
                warning() << "Error recalculating received_amount for path "
                          << subsequentPathID << ": " << e.what();
                continue;
            }
        }

        // Step 10: Update path parameters
        pathStats->optimal_flow = finalOptimalFlow;
        pathStats->received_amount = finalReceivedAmount;

        info() << "Updated path " << subsequentPathID
               << ": optimal_flow=" << finalOptimalFlow
               << ", receivedAmount changed from " << oldReceivedAmount
               << " to " << finalReceivedAmount;

        // Step 11: Recalculate flows
        try {
            pathStats->calculateFlows(finalOptimalFlow);
            info() << "Recalculated flows for path " << subsequentPathID;
        } catch (const exception &e) {
            warning() << "Error recalculating flows for path "
                      << subsequentPathID << ": " << e.what();
        }

        prepareCommissionsForPath(pathStats, subsequentPathID);

        // Note: exchangeSteps have been updated in Step 7.5
        // Other ExchangePath fields (minCapacity, effectiveExchangeRate, totalCommissions)
        // are recalculated lazily or don't need explicit updates for subsequent paths
    }

    info() << "Completed updating subsequent paths affected by condition change";
}

/**
 * Calculates the final amount received at the destination after applying updated exchange rates
 * and/or commissions along a payment path.
 *
 * This method performs a forward simulation through the payment path, starting from the coordinator
 * with the input flow amount and applying all exchanges and commissions step by step. At the affected
 * position, it uses the updated conditions (rate and/or commission) instead of the original values
 * from path structure.
 *
 * Algorithm:
 * 1. Start with inputFlow amount at coordinator (position 0)
 * 2. For each step in path.ids:
 *    a. If exchange detected (same node ID, different equivalents):
 *       - Use updated rate if idx == affectedPositionInPath
 *       - Otherwise use original rate from path.exchangeSteps
 *       - Apply exchange: amount = amount * rate * 10^shift
 *    b. If commission detected (same node ID and equivalent, commission > 0):
 *       - Use updated commission if idx == affectedPositionInPath
 *       - Otherwise use original commission from path.exchangeSteps
 *       - Apply commission: amount = amount - commission
 * 3. Return final amount (what receiver gets)
 *
 * Path traversal details:
 * - Iterate through path.ids pairs: (ids[i], ids[i+1]) with (equivalents[i], equivalents[i+1])
 * - Exchange occurs when: ids[i] == ids[i+1] && equivalents[i] != equivalents[i+1]
 * - Commission occurs at any intermediate position (0 < idx < ids.size()-1) where exchangeStep exists
 *   with fromEquivalent == toEquivalent
 *
 * @param pathStats Pointer to OptimalPathResult containing the path structure with nodes, equivalents,
 *                  and exchangeSteps
 * @param inputFlow The amount entering the path at the coordinator (before any exchanges/commissions)
 * @param affectedPositionInPath The index in path.ids where updated conditions should be applied
 *                              (0-based index corresponding to the node position in path.ids)
 * @param updatedRate Optional new exchange rate (mantissa, exponent) to use at affectedPositionInPath.
 *                   If nullopt, use original rate from path.exchangeSteps
 * @param updatedCommission Optional new commission to use at affectedPositionInPath.
 *                         If nullopt, use original commission from path.exchangeSteps
 * @return The final amount that will be received at the destination after all exchanges and commissions
 * @throws ValueError if exchange step not found or if amount is exhausted by commission
 */
const string CoordinatorExchangePaymentTransaction::logHeader() const
{
    stringstream s;
    s << "[CoordinatorExchangePaymentTA: " << currentTransactionUUID().stringUUID() << " " << mEquivalent << "] ";
    return s.str();
}
