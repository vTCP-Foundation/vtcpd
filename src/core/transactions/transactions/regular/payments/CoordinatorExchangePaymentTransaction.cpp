#include "CoordinatorExchangePaymentTransaction.h"

#include <map>

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
    mCountReceiverInaccessible(0)
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

    // Calculate mExchangeAmount (amount to pay in sender equivalent)
    // using cached paths similar to EstimatePaymentForReceiveAmountTransaction
    try {
        PathCacheKey key{mContractorID, mExchangeEquivalent, mEquivalent};
        auto cachedPaths = mExchangePathsManager->retrievePaths(key);

        if (!cachedPaths) {
            warning() << "No cached optimal paths for contractor " << mContractorID
                      << " with sender_eq=" << mExchangeEquivalent
                      << " and receiver_eq=" << mEquivalent;
            return resultNoPathsError();
        }

        // Calculate required payment amount (simplified approach without inverseSimulatePath)
        TrustLineAmount remainingReceive = mCommand->amount();  // mAmount - receiver amount
        TrustLineAmount totalPayment = TrustLineAmount(0);

        for (const auto &pathResult : *cachedPaths) {
            if (remainingReceive == TrustLineAmount(0)) {
                break;
            }

            // Use the minimum of remaining needed and what this path can deliver
            TrustLineAmount deliveredAmount = min(remainingReceive, pathResult.received_amount);

            // For simplified calculation: assume optimal_flow is proportional to received_amount
            // More accurate would be to use inverseSimulatePath, but for now use direct ratio
            double ratio = deliveredAmount.convert_to<double>() / pathResult.received_amount.convert_to<double>();
            TrustLineAmount requiredPayment(static_cast<uint64_t>(pathResult.optimal_flow.convert_to<double>() * ratio));

            totalPayment = totalPayment + requiredPayment;
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

    // mResourcesManager->requestPaths(
    //     currentTransactionUUID(),
    //     mContractor->mainAddress(),
    //     mEquivalent);

    mStep = Stages::Coordinator_ReceiverResourceProcessing;
    return resultWaitForResourceTypes(
    {BaseResource::Paths},
    // this delay should be greater than time of FindPathByMaxFlowTransaction running,
    // because we didn't get resources
    maxNetworkDelay(4));
}

TransactionResult::SharedConst CoordinatorExchangePaymentTransaction::runPathsResourceProcessingStage()
{
    debug() << "runPathsResourceProcessingStage";

    // Step 1: Initialize total flow counter
    TrustLineAmount remainingAmount = mExchangeAmount;
    TrustLineAmount totalAddedFlow = TrustLineAmount(0);

    // Step 2: Iterate through each sender equivalent
    for (const auto& senderEquiv : mExchangeEquivalents) {
        // Step 3: Create cache key for this sender-receiver equivalent combination
        PathCacheKey key{
            mContractorID,
            senderEquiv,
            mEquivalent  // receiver equivalent
        };

        // Step 4: Retrieve optimal paths from ExchangePathsManager
        auto optimalPaths = mExchangePathsManager->retrievePaths(key);

        if (!optimalPaths) {
            // No paths available for this equivalent combination
            debug() << "No cached paths for sender equiv " << senderEquiv;
            continue;
        }

        // Step 5: Add paths until total flow >= payment amount (in receiver equivalent)
        for (const auto& pathResult : *optimalPaths) {
            if (remainingAmount == TrustLineAmount(0)) {
                break;  // Sufficient flow accumulated
            }

            // Step 6: Add path to mPathsStats
            const auto pathAmount = min(remainingAmount, pathResult.optimal_flow);
            addPathForFurtherProcessing(pathResult, pathAmount);
            remainingAmount = remainingAmount - pathAmount;
            totalAddedFlow = totalAddedFlow + pathResult.received_amount;
        }

        // Check if we have enough flow
        if (remainingAmount == TrustLineAmount(0)) {
            break;  // No need to check other equivalents
        }
    }

    // Step 7: Validate that we have sufficient paths (check receiver amount delivery)
    if (remainingAmount > TrustLineAmount(0)) {
        warning() << "Insufficient total flow: " << totalAddedFlow
                  << " < " << mAmount;
        return transactionResultFromCommand(
            mCommand->responseInsufficientFunds());
    }

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

    if (kMessage->amountReserved() != pathStats->maxFlow()) {
        pathStats->shortageMaxFlow(
            kMessage->amountReserved());
        shortageReservationsOnPath(
            receiverID,
            mCurrentAmountReservingPathIdentifier,
            pathStats->maxFlow());
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

    mCurrentAmountReservingPathIdentifier = *mPathIDs.cbegin();
    debug() << "[" << mCurrentAmountReservingPathIdentifier << "] path reservation initialized";
    mCurrentPathParticipants.clear();
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
    pathStats->shortageMaxFlow(kReservationAmount);
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

    if (!senderTrustLines->trustLineOwnKeysPresent(neighborID)) {
        warning() << "There are no own keys on TL with neighbor. Switching to another path.";
        path->setUnusable();
        mNeighborsKeysProblem = true;
        publicKeysSharingSignal(neighborID, senderEquivalent);
        mIsAuditPendingPathsOccurred = true;
        throw CallChainBreakException("Break call chain for preventing call loop");
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

    // const auto kAvailableOutgoingAmount = senderTrustLines->outgoingTrustAmountConsideringReservations(neighborID);
    // const auto kRemainingAmountForProcessing =
    //     mExchangeAmount - totalReservedAmount(AmountReservation::Outgoing, senderEquivalent);

    // const auto kReservationAmount = min(*kAvailableOutgoingAmount, kRemainingAmountForProcessing);
    const auto kReservationAmount = path->paymentFlow;

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

    path->shortageMaxFlow(kReservationAmount);
    path->setNodeState(
        kFirstIntermediateNodeIndex,
        OptimalPathResult::NeighbourReservationRequestSent);

    vector<PathReservation> reservations;
    reservations.emplace_back(
        mCurrentAmountReservingPathIdentifier,
        make_shared<const TrustLineAmount>(kReservationAmount),
        senderEquivalent);

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
        pathFlow.second);

    // TODO: Add existing next after neighbor node reservations from mNodesFinalAmountsConfiguration
    // For now, skip this optimization
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
        pathFlow.second);

    // TODO: Add existing next after remote node reservations from mNodesFinalAmountsConfiguration
    // For now, skip this optimization

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
            << pathStats->maxFlow() << ", next node - (" << nextAfterRemoteNode->fullAddress() << ")]";

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

    if (message->amountReserved() != path->maxFlow()) {
        path->shortageMaxFlow(message->amountReserved());
        shortageReservationsOnPath(
            neighborID,
            mCurrentAmountReservingPathIdentifier,
            path->maxFlow());
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
        path->shortageMaxFlow(message->amountReserved());
        debug() << "Path max flow is now " << path->maxFlow();
        shortageReservationsOnPath(
            neighborID,
            mCurrentAmountReservingPathIdentifier,
            path->maxFlow());
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

        // send final path amount to all intermediate nodes on path
        sendFinalPathConfiguration(
            path,
            mCurrentAmountReservingPathIdentifier,
            path->maxFlow());

        try {
            addFinalConfigurationOnPath(
                mCurrentAmountReservingPathIdentifier,
                path);
        } catch (const ValueError& e) {
            error() << "Failed to add final configuration: " << e.what();
            return reject("Internal payment error: flow calculation mismatch");
        }

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

    if (reservedAmount != path->maxFlow()) {
        path->shortageMaxFlow(reservedAmount);
        auto firstIntermediateNode = pathNodes[0];
        auto firstIntermediateNodeID = mContractorsManager->contractorIDByAddress(firstIntermediateNode);
        shortageReservationsOnPath(
            firstIntermediateNodeID,
            mCurrentAmountReservingPathIdentifier,
            path->maxFlow());
        debug() << "Path max flow is now " << path->maxFlow();
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

        // send final path amount to all intermediate nodes on path
        sendFinalPathConfiguration(
            path,
            mCurrentAmountReservingPathIdentifier,
            path->maxFlow());

        try {
            addFinalConfigurationOnPath(
                mCurrentAmountReservingPathIdentifier,
                path);
        } catch (const ValueError& e) {
            error() << "Failed to add final configuration: " << e.what();
            return reject("Internal payment error: flow calculation mismatch");
        }

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

    if (mPathIDs.empty()) {
        // remove unusable path from paths scope
        if (!justProcessedPath->isValid()) {
            mPathsStats.erase(justProcessedPathIdentifier);
        }
        throw NotFoundError(
            "CoordinatorExchangePaymentTransaction::switchToNextPath: "
            "no paths are available");
    }

    mCurrentAmountReservingPathIdentifier = *mPathIDs.cbegin();
    debug() << "[" << mCurrentAmountReservingPathIdentifier << "] switching to next path";

    // remove unusable path from paths scope
    if (!justProcessedPath->isValid()) {
        mPathsStats.erase(justProcessedPathIdentifier);
    }
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
    debug() << "shortageReservationsOnPath";
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
        for (const auto &intermediateNode : pathStats->path().nodes) {
            if (!sendToLastProcessedNode && intermediateNode == lastProcessedNode) {
                break;
            }
            debug() << "send message with drop reservation info for node " << intermediateNode->fullAddress();
            sendMessage<FinalPathConfigurationMessage>(
                intermediateNode,
                senderEquivalent,
                mContractorsManager->ownAddresses(),
                currentTransactionUUID(),
                pathID,
                TrustLine::kZeroAmount());
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
    mPathsStats[pathID] = std::move(pathCopy);
    mPathIDs.push_back(pathID);

    debug() << "Path " << pathID << " added with "
            << pathLength << " nodes, flow: " << pathResult.optimal_flow;
    debug() << "Path " << pathID << " " << mPathsStats[pathID]->path().toString();
    for (const auto &flow : mPathsStats[pathID]->flows) {
        debug() << "Flow: " << flow.first << ", Equivalent: " << flow.second;
    }
    for (const auto &nodeState : mPathsStats[pathID]->mIntermediateNodesStates) {
        debug() << "Node state: " << nodeState;
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
                incomingFlow.second);
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
                outgoingFlow.second);
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
        receiverIncomingFlow.second);
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
    const PathID &pathID,
    const TrustLineAmount &finalPathAmount)
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
        debug() << "send message with final path amount info for node " << intermediateNode->fullAddress();
        sendMessage<FinalPathConfigurationMessage>(
            intermediateNode,
            senderEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            pathID,
            finalPathAmount);
    }
}

const string CoordinatorExchangePaymentTransaction::logHeader() const
{
    stringstream s;
    s << "[CoordinatorExchangePaymentTA: " << currentTransactionUUID().stringUUID() << " " << mEquivalent << "] ";
    return s.str();
}
