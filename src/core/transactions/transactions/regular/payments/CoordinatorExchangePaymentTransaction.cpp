#include "CoordinatorExchangePaymentTransaction.h"
#include "../../../network/messages/payments/FinalPathExchangeConfigurationMessage.h"

#include <map>
#include <algorithm>
#include <string>
#include <boost/multiprecision/cpp_int.hpp>

#include "../../../../common/exceptions/ValueError.h"

using boost::multiprecision::cpp_int;

namespace {

cpp_int pow10(const size_t exponent)
{
    cpp_int result = 1;
    for (size_t idx = 0; idx < exponent; ++idx) {
        result *= 10;
    }
    return result;
}

TrustLineAmount ceilDivideToAmount(const cpp_int &numerator, const cpp_int &denominator)
{
    if (denominator <= 0) {
        throw ValueError(
            "CoordinatorExchangePaymentTransaction::ceilDivideToAmount: "
            "denominator must be positive");
    }

    cpp_int quotient = numerator / denominator;
    if (numerator % denominator != 0) {
        ++quotient;
    }

    if (quotient < 0) {
        throw ValueError(
            "CoordinatorExchangePaymentTransaction::ceilDivideToAmount: "
            "negative quotient computed");
    }

    return quotient.convert_to<TrustLineAmount>();
}

const ExchangeStep* findExchangeStep(
    const ExchangePath &path,
    ContractorID nodeID,
    SerializedEquivalent fromEquivalent,
    SerializedEquivalent toEquivalent)
{
    const auto it = std::find_if(
        path.exchangeSteps.begin(),
        path.exchangeSteps.end(),
        [nodeID, fromEquivalent, toEquivalent](const ExchangeStep &step) {
            return step.nodeID == nodeID &&
                   step.fromEquivalent == fromEquivalent &&
                   step.toEquivalent == toEquivalent;
        });

    if (it == path.exchangeSteps.end()) {
        return nullptr;
    }

    return &(*it);
}

TrustLineAmount invertExchangeForRequiredInput(
    const ExchangeStep &step,
    const TrustLineAmount &outputAmount)
{
    if (step.exchangeRate == TrustLineAmount(0)) {
        throw ValueError(
            "CoordinatorExchangePaymentTransaction::invertExchangeForRequiredInput: "
            "zero exchange rate encountered");
    }

    cpp_int numerator = cpp_int(outputAmount);
    cpp_int denominator = cpp_int(step.exchangeRate);

    if (step.exchangeRateShift >= 0) {
        denominator *= pow10(static_cast<size_t>(step.exchangeRateShift));
    } else {
        numerator *= pow10(static_cast<size_t>(-step.exchangeRateShift));
    }

    return ceilDivideToAmount(numerator, denominator);
}

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
            const auto *exchangeStep = findExchangeStep(
                path,
                currentNode,
                previousEquivalent,
                currentEquivalent);
            if (!exchangeStep) {
                throw ValueError(
                    "CoordinatorExchangePaymentTransaction::calculateRequiredInputForPath: "
                    "exchange step not found");
            }

            requiredAmount = invertExchangeForRequiredInput(*exchangeStep, requiredAmount);
            continue;
        }

        if (idx < path.ids.size() - 1) {
            const auto *commissionStep = findExchangeStep(
                path,
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

TrustLineAmount applyExchangeForward(
    const TrustLineAmount &amount,
    const ExchangeStep &step)
{
    cpp_int result = cpp_int(amount) * cpp_int(step.exchangeRate);

    if (step.exchangeRateShift >= 0) {
        result *= pow10(static_cast<size_t>(step.exchangeRateShift));
    } else {
        const cpp_int divisor = pow10(static_cast<size_t>(-step.exchangeRateShift));
        result /= divisor;
    }

    if (result < 0) {
        throw ValueError(
            "CoordinatorExchangePaymentTransaction::applyExchangeForward: "
            "negative exchange result");
    }

    return result.convert_to<TrustLineAmount>();
}

}

CoordinatorExchangePaymentTransaction::CoordinatorExchangePaymentTransaction(
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
    mIsAuditPendingPathsOccurred(false),
    mCountReceiverInaccessible(0),
    mIsWaitingForExchangePathsResource(false)
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

        for (auto pathResult : *cachedPaths) {  // Copy to allow modification
            if (remainingReceive == TrustLineAmount(0)) {
                break;
            }

            TrustLineAmount deliveredAmount = min(remainingReceive, pathResult.received_amount);
            if (deliveredAmount == TrustLineAmount(0)) {
                continue;
            }

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
    const auto senderEquivalent = pathStats->mPath.equivalents.empty() ? mEquivalent : pathStats->mPath.equivalents.front();

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

        // Step 3: Validate path for processing (check inaccessible nodes and rejected trust lines)
        if (!validatePathForProcessing(pathStats)) {
            // Path contains bad nodes/trustlines, mark unusable and try next
            pathStats->setUnusable();
            mPathsStats.erase(nextPathID);
            mPathIDs.erase(mPathIDs.cbegin());
            continue;
        }

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

                // Recalculate flows for reservation
                pathStats->calculateFlows(truncatedInput);

                info() << "Path capacity truncated: input=" << truncatedInput
                       << ", output=" << remainingNeeded;

            } catch (const std::exception &e) {
                // Truncation calculation failed, skip this path and try next
                error() << "Path capacity truncation failed: " << e.what()
                        << ", skipping path";
                pathStats->setUnusable();
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
    const auto senderEquivalent = pathStats->mPath.equivalents.empty() ? mEquivalent : pathStats->mPath.equivalents.front();
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
    const auto senderEquivalent = path->currentPathEquivalent();
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

#ifdef TESTS
    mSubsystemsController->testForbidSendMessageToCoordinatorOnReservationStage(
        neighbor,
        pathFlow.first);
#endif

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

#ifdef TESTS
    mSubsystemsController->testForbidSendMessageToCoordinatorOnReservationStage(
        remoteNode,
        pathFlow.first);
#endif

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
    const auto senderEquivalent = kPathStats->mPath.equivalents.empty() ? mEquivalent : kPathStats->mPath.equivalents.front();
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
        const auto senderEquivalent = path->mPath.equivalents.empty() ? mEquivalent : path->mPath.equivalents.front();
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
        const auto senderEquivalent = path->mPath.equivalents.empty() ? mEquivalent : path->mPath.equivalents.front();
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

    // Switch to next path
    switchToNextPath();

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
        mPathsStats.erase(justProcessedPathIdentifier);
    }

    // Step 1: Check if more capacity needed before taking next path
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

        // Step 3: Validate path for processing (check inaccessible nodes and rejected trust lines)
        if (!validatePathForProcessing(pathStats)) {
            // Path contains bad nodes/trustlines, mark unusable and try next
            pathStats->setUnusable();
            mPathsStats.erase(nextPathID);
            mPathIDs.erase(mPathIDs.cbegin());
            continue;
        }

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

                // Recalculate flows for reservation
                pathStats->calculateFlows(truncatedInput);

                info() << "Path capacity truncated: input=" << truncatedInput
                       << ", output=" << remainingNeeded;

            } catch (const std::exception &e) {
                // Truncation calculation failed, skip this path and try next
                error() << "Path capacity truncation failed: " << e.what()
                        << ", skipping path";
                pathStats->setUnusable();
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
        // because findExchangeStep expects ContractorID, not BaseAddress

        TrustLineAmount currentAmount = kNewAmount;

        for (size_t idx = 0; idx + 1 < path.ids.size(); ++idx) {
            const ContractorID fromNode = path.ids[idx];
            const ContractorID toNode = path.ids[idx + 1];
            const SerializedEquivalent currentEquiv = path.equivalents[idx];
            const SerializedEquivalent nextEquiv = path.equivalents[idx + 1];

            // Check for exchange (same node, different equivalent)
            if (fromNode == toNode && currentEquiv != nextEquiv) {
                // Find exchange step
                const auto *exchangeStep = findExchangeStep(
                    path, fromNode, currentEquiv, nextEquiv);

                if (!exchangeStep) {
                    warning() << "Exchange step not found during shortage calculation";
                    return;  // Keep old values
                }

                // Apply exchange forward
                currentAmount = applyExchangeForward(currentAmount, *exchangeStep);
                continue;  // Don't add to flows, this is in-place exchange
            }

            // Check for commission at arrival node (if not receiver)
            if (idx + 1 < path.ids.size() - 1) {  // Not last node (receiver)
                const auto *commissionStep = findExchangeStep(
                    path, toNode, nextEquiv, nextEquiv);

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
    pathStats->mMaxPathFlow = kNewAmount;
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

    // Get equivalent from path
    const auto senderEquivalent = pathStats->mPath.equivalents.empty() ? mEquivalent : pathStats->mPath.equivalents.front();
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

        // Determine equivalents from path based on node position
        const auto &pathNodes = pathStats->path().nodes;
        const auto &pathEquivalents = pathStats->mPath.equivalents;

        if (pathEquivalents.size() != pathNodes.size()) {
            warning() << "Path equivalents size mismatch: " << pathEquivalents.size()
                      << " vs nodes size: " << pathNodes.size();
            return;
        }

        for (size_t nodeIdx = 0; nodeIdx < pathNodes.size(); ++nodeIdx) {
            const auto &intermediateNode = pathNodes[nodeIdx];

            if (!sendToLastProcessedNode && intermediateNode == lastProcessedNode) {
                break;
            }

            // Determine incoming and outgoing equivalents based on node position
            SerializedEquivalent incomingEquivalent =
                (nodeIdx > 0) ? pathEquivalents[nodeIdx - 1] : senderEquivalent;
            SerializedEquivalent outgoingEquivalent = pathEquivalents[nodeIdx];

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
    // pathCopy->mPath.nodes.reserve(pathLength);

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
    pathCopy->mMaxPathFlow = pathResult.optimal_flow;
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
}

void CoordinatorExchangePaymentTransaction::sendFinalPathConfiguration(
    OptimalPathResult *pathStats,
    const PathID &pathID)
{
    debug() << "sendFinalPathConfiguration";

    // Determine the sender equivalent (first equivalent in the path, or use mExchangeEquivalent as fallback)
    const auto senderEquivalent = pathStats->mPath.equivalents.empty() ? mExchangeEquivalent : pathStats->mPath.equivalents.front();

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

TrustLineAmount CoordinatorExchangePaymentTransaction::calculateTotalReservedAmount()
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
            isReserved = (pathStats->mMaxPathFlow > TrustLineAmount(0));
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

const string CoordinatorExchangePaymentTransaction::logHeader() const
{
    stringstream s;
    s << "[CoordinatorExchangePaymentTA: " << currentTransactionUUID().stringUUID() << " " << mEquivalent << "] ";
    return s.str();
}
