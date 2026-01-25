#include "IntermediateNodeExchangePaymentTransaction.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>

#include "../../../../common/exceptions/ValueError.h"
#include "../../../../common/multiprecision/ExchangeRateMath.h"

IntermediateNodeExchangePaymentTransaction::IntermediateNodeExchangePaymentTransaction(
    IntermediateNodeReservationRequestMessage::ConstShared message,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    StorageHandler *storageHandler,
    ResourcesManager *resourcesManager,
    ExchangeRatesManager *exchangeRatesManager,
    CommissionsManager *commissionsManager,
    ExchangePathsManager *exchangePathsManager,
    ObservingHandler *observingHandler,
    Keystore *keystore,
    Logger &log,
    SubsystemsController *subsystemsController) :

    BaseExchangePaymentTransaction(
        BaseTransaction::IntermediateNodeExchangePaymentTransaction,
        message->transactionUUID(),
        message->equivalent(),
        contractorsManager,
        equivalentsSubsystemsRouter,
        storageHandler,
        resourcesManager,
        exchangePathsManager,
        observingHandler,
        keystore,
        log,
        subsystemsController),
    mMessage(message),
    mExchangeRatesManager(exchangeRatesManager),
    mCommissionsManager(commissionsManager)
{
    mStep = Stages::IntermediateNode_PreviousNeighborRequestProcessing;
    mCoordinator = nullptr;
    mCreationTime = utc_now();
}

IntermediateNodeExchangePaymentTransaction::IntermediateNodeExchangePaymentTransaction(
    BytesShared buffer,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    StorageHandler *storageHandler,
    ResourcesManager *resourcesManager,
    ExchangeRatesManager *exchangeRatesManager,
    CommissionsManager *commissionsManager,
    ExchangePathsManager *exchangePathsManager,
    ObservingHandler *observingHandler,
    Keystore *keystore,
    Logger &log,
    SubsystemsController *subsystemsController) :

    BaseExchangePaymentTransaction(
        buffer,
        contractorsManager,
        equivalentsSubsystemsRouter,
        storageHandler,
        resourcesManager,
        exchangePathsManager,
        observingHandler,
        keystore,
        log,
        subsystemsController),
    mExchangeRatesManager(exchangeRatesManager),
    mCommissionsManager(commissionsManager)
{
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::run()
{
    try {
        switch (mStep) {
        case Stages::IntermediateNode_PreviousNeighborRequestProcessing:
            return runPreviousNeighborRequestProcessingStage();

        case Stages::IntermediateNode_CoordinatorRequestProcessing:
            return runCoordinatorRequestProcessingStage();

        case Stages::IntermediateNode_NextNeighborResponseProcessing:
            return runNextNeighborResponseProcessingStage();

        case Stages::Common_FinalPathConfigurationChecking:
            return runFinalPathConfigurationProcessingStage();

        case Stages::IntermediateNode_ReservationProlongation:
            return runReservationProlongationStage();

        case Stages::Coordinator_FinalAmountsConfigurationConfirmation:
            return runFinalAmountsConfigurationConfirmation();

        case Stages::Common_ObservingBlockNumberProcessing:
            return runCheckObservingBlockNumber();

        case Stages::Common_Voting:
            return runVotesCheckingStageWithCoordinatorClarification();

        case Stages::Common_VotesChecking:
            return runVotesConsistencyCheckingStage();

        case Stages::Common_Recovery:
            return runVotesRecoveryParentStage();

        // Observing stages are handled by the transaction state machine.
        case Stages::Observing_AcceptClaim:
            return runObservingAcceptClaimStage();

        case Stages::Observing_GetClaimStatus:
            return runObservingGetClaimStatusStage();

        case Stages::Common_Observing:
            return runObservingResultStage();

        case Stages::Common_ObservingReject:
            return runObservingRejectTransaction();

        case Stages::Common_Uncertain: {
            info() << "Uncertain stage";
            clearContextIfTransactionIdle();
            if (!canCompleteTransaction()) {
                warning() << "Uncertain stage reached with pending reservations or context";
            }
            return resultDone();
        }

        default:
            throw RuntimeError(
                "IntermediateNodeExchangePaymentTransaction::run: "
                "unexpected stage occurred.");
        }
    } catch (Exception &e) {
        warning() << e.what();
        return recover("Something happens wrong in method run(). Transaction will be recovered");
    }
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::runPreviousNeighborRequestProcessingStage()
{
    debug() << "runPreviousNeighborRequestProcessingStage";
    const auto kNeighbor = mContractorsManager->contractorAddresses(mMessage->idOnReceiverSide).at(0);
    debug() << "Init. intermediate payment operation from node (" << kNeighbor->fullAddress() << ")";

    const auto transactionDuration = utc_now() - mCreationTime;
    if (transactionDuration.total_seconds() > kMaxTransactionDurationSeconds) {
        return sendErrorMessageOnPreviousNodeRequest(
            kNeighbor,
            0,                  // 0, because we don't know pathID
            ResponseMessage::Closed);
        return reject("Transaction duration time has expired. Rolling back");
    }

    const auto &kFinalAmounts = mMessage->finalAmountsConfiguration();
    if (kFinalAmounts.empty()) {
        warning() << "Not received reservation";
        return sendErrorMessageOnPreviousNodeRequest(
                   kNeighbor,
                   0,                  // 0, because we don't know pathID
                   ResponseMessage::Closed);
    }

    const auto &kReservation = kFinalAmounts[0];
    debug() << "Requested amount reservation: " << *kReservation.amount.get()
            << " on path " << kReservation.pathID
            << " equivalent " << kReservation.equivalent;
    debug() << "Received reservations size: " << kFinalAmounts.size();

    auto senderID = mContractorsManager->contractorIDByAddress(kNeighbor);
    if (senderID == ContractorsManager::kNotFoundContractorID) {
        warning() << "Previous node is not a neighbor";
        return sendErrorMessageOnPreviousNodeRequest(
                   kNeighbor,
                   kReservation.pathID,
                   ResponseMessage::Rejected);
    }
    debug() << "Sender ID " << senderID;

    auto *const trustLines = trustLinesManager(kReservation.equivalent);

    if (!trustLines->trustLineIsPresent(senderID)) {
        warning() << "Path is not valid: there is no TL with previous node. Rejected.";
        return sendErrorMessageOnPreviousNodeRequest(
                   kNeighbor,
                   kReservation.pathID,
                   ResponseMessage::Rejected);
    }

    if (!trustLines->trustLineIsActive(senderID)) {
        warning() << "Path is not valid: TL with previous node is not active. Rejected.";
        if (trustLines->trustLineState(senderID) == TrustLine::AuditPending ||
                trustLines->trustLineState(senderID) == TrustLine::KeysSharing) {
            info() << "Due to audit pending";
            return sendErrorMessageOnPreviousNodeRequest(
                       kNeighbor,
                       kReservation.pathID,
                       ResponseMessage::RejectedDueAuditPending);
        }
        return sendErrorMessageOnPreviousNodeRequest(
                   kNeighbor,
                   kReservation.pathID,
                   ResponseMessage::Rejected);
    }

    if (!trustLines->trustLineOwnKeysPresent(senderID)) {
        warning() << "There are no own keys. Rejected";
        publicKeysSharingSignal(senderID, kReservation.equivalent);
        return sendErrorMessageOnPreviousNodeRequest(
                   kNeighbor,
                   kReservation.pathID,
                   ResponseMessage::RejectedDueOwnKeysAbsence);
    }

    if (!trustLines->trustLineContractorKeysPresent(senderID)) {
        warning() << "There are no contractor keys. Rejected";
        return sendErrorMessageOnPreviousNodeRequest(
                   kNeighbor,
                   kReservation.pathID,
                   ResponseMessage::RejectedDueContractorKeysAbsence);
    }

    if (!updateReservations(vector<PathReservation>(
                                kFinalAmounts.begin() + 1,
                                kFinalAmounts.end()))) {
        warning() << "Previous node send path configuration, which is absent on current node";
        for (const auto &reservation : kFinalAmounts) {
            debug() << "path: " << reservation.pathID
                    << " amount: " << *reservation.amount.get()
                    << " equiv: " << reservation.equivalent;
        }
        return sendErrorMessageOnPreviousNodeRequest(
                   kNeighbor,
                   kReservation.pathID,
                   ResponseMessage::Closed);
    }
    debug() << "All reservations were updated";

    const auto kIncomingAmount = trustLines->incomingTrustAmountConsideringReservations(senderID);
    TrustLineAmount reservationAmount = std::min(
                                            *kReservation.amount.get(),
                                            *kIncomingAmount);

#ifdef TESTS
    mSubsystemsController->testForbidSendResponseToIntNodeOnReservationStage(
        kNeighbor,
        reservationAmount);
    mSubsystemsController->testThrowExceptionOnPreviousNeighborRequestProcessingStage();
    mSubsystemsController->testTerminateProcessOnPreviousNeighborRequestProcessingStage();
#endif

    if (reservationAmount == TrustLineAmount(0) ||
            !reserveIncomingAmount(
                senderID,
                reservationAmount,
                kReservation.pathID,
                kReservation.equivalent)) {
        warning() << "No amount reservation is possible.";
        return sendErrorMessageOnPreviousNodeRequest(
                   kNeighbor,
                   kReservation.pathID,
                   ResponseMessage::Rejected);
    }

    debug() << "reserve locally " << reservationAmount << " to node "
            << kNeighbor->fullAddress() << " on path " << kReservation.pathID;
    mLastReservedAmount = reservationAmount;
    mLastProcessedPath = kReservation.pathID;
    sendMessage<IntermediateNodeReservationResponseMessage>(
        kNeighbor,
        kReservation.equivalent,
        mContractorsManager->ownAddresses(),
        currentTransactionUUID(),
        kReservation.pathID,
        ResponseMessage::Accepted,
        reservationAmount);

    mStep = Stages::IntermediateNode_CoordinatorRequestProcessing;
    return resultWaitForMessageTypes( {
        Message::Payments_CoordinatorReservationRequest,
        Message::Payments_IntermediateNodeReservationRequest,
        Message::Payments_FinalAmountsConfiguration,
        Message::Payments_TransactionPublicKeyHash,
        Message::Payments_TTLProlongationResponse},
    maxNetworkDelay(4));
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::runCoordinatorRequestProcessingStage()
{
    debug() << "runCoordinatorRequestProcessingStage";

    if (!contextIsValid(Message::Payments_CoordinatorReservationRequest, false)) {
        warning() << "No coordinator request received.";
        return runReservationProlongationStage();
    }

    debug() << "Coordinator further reservation request received.";

#ifdef TESTS
    mSubsystemsController->testThrowExceptionOnCoordinatorRequestProcessingStage();
    mSubsystemsController->testTerminateProcessOnCoordinatorRequestProcessingStage();
#endif

    const auto kMessage = popNextMessage<CoordinatorReservationRequestMessage>();
    mCoordinator = make_shared<Contractor>(kMessage->senderAddresses);
    const auto kNextNode = kMessage->nextNodeInPath();
    const auto &kFinalAmounts = kMessage->finalAmountsConfiguration();
    if (kFinalAmounts.empty()) {
        warning() << "Not received reservation";
        return sendErrorMessageOnCoordinatorRequest(
                   ResponseMessage::Closed);
    }

    const auto &kReservation = kFinalAmounts[0];
    mLastProcessedPath = kReservation.pathID;

    const auto expectedExchangeRate = kMessage->expectedExchangeRate();
    const auto expectedCommission = kMessage->expectedCommission();

    if (expectedExchangeRate.has_value() && expectedCommission.has_value()) {
        error() << "Protocol violation: both exchange rate and commission provided in request";
        return sendErrorMessageOnCoordinatorRequest(
                   ResponseMessage::Rejected);
    }

    debug() << "requested reservation amount is " << *kReservation.amount.get()
            << " on path " << kReservation.pathID
            << " equivalent " << kReservation.equivalent;
    debug() << "Next node is " << kNextNode->fullAddress();
    debug() << "Received reservations size: " << kFinalAmounts.size();
    auto nextNodeID = mContractorsManager->contractorIDByAddress(kNextNode);
    if (nextNodeID == ContractorsManager::kNotFoundContractorID) {
        warning() << "Next node is not neighbor";
        return sendErrorMessageOnCoordinatorRequest(
                   ResponseMessage::Rejected);
    }
    info() << "Next node ID " << nextNodeID;

    auto *const trustLines = trustLinesManager(kReservation.equivalent);

    if (!trustLines->trustLineIsPresent(nextNodeID)) {
        warning() << "Path is not valid: there is no TL with next node. Rolled back.";
        return sendErrorMessageOnCoordinatorRequest(
                   ResponseMessage::Rejected);
    }

    if (!trustLines->trustLineIsActive(nextNodeID)) {
        warning() << "Path is not valid: TL with next node is not active. Rolled back.";
        if (trustLines->trustLineState(nextNodeID) == TrustLine::AuditPending ||
                trustLines->trustLineState(nextNodeID) == TrustLine::KeysSharing) {
            info() << "Due to audit pending";
            return sendErrorMessageOnCoordinatorRequest(
                       ResponseMessage::RejectedDueAuditPending);
        }
        return sendErrorMessageOnCoordinatorRequest(
                   ResponseMessage::Rejected);
    }

    if (!trustLines->trustLineOwnKeysPresent(nextNodeID)) {
        warning() << "There are no own keys on TL";
        publicKeysSharingSignal(nextNodeID, kReservation.equivalent);
        return sendErrorMessageOnCoordinatorRequest(
                   ResponseMessage::RejectedDueOwnKeysAbsence);
    }

    if (!trustLines->trustLineContractorKeysPresent(nextNodeID)) {
        warning() << "There are no contractor keys on TL";
        return sendErrorMessageOnCoordinatorRequest(
                   ResponseMessage::RejectedDueContractorKeysAbsence);
    }

    // Locate incoming reservation for the same path to validate conditions
    AmountReservation::ConstShared incomingReservation = nullptr;
    SerializedEquivalent incomingEquivalent = 0;
    for (const auto &[contractorID, reservationsMap] : mReservations) {
        for (const auto &[pathID, reservation] : reservationsMap) {
            if (pathID == kReservation.pathID &&
                reservation->direction() == AmountReservation::Incoming) {
                incomingReservation = reservation;
                incomingEquivalent = reservation->equivalent();
                break;
            }
        }
        if (incomingReservation) {
            break;
        }
    }

    if (!incomingReservation) {
        error() << "Incoming reservation not found for path " << kReservation.pathID;
        return sendErrorMessageOnCoordinatorRequest(
                   ResponseMessage::Rejected);
    }

    const auto outgoingEquivalent = kReservation.equivalent;
    const auto incomingAmount = incomingReservation->amount();
    const TrustLineAmount requestedOutgoingAmount = *kReservation.amount.get();
    const bool equivalentsDiffer = incomingEquivalent != outgoingEquivalent;
    const auto rateKey = make_pair(incomingEquivalent, outgoingEquivalent);
    const bool commissionAlreadyCharged =
        mChargedCommissionEquivalents.find(incomingEquivalent) != mChargedCommissionEquivalents.end();

    std::optional<ExchangeRate::Shared> actualExchangeRate;
    std::optional<Commission::Shared> actualCommission;

    if (expectedExchangeRate.has_value()) {
        if (!equivalentsDiffer) {
            warning() << "Exchange rate provided for identical equivalents";
        }

        auto contextIt = mContextExchangeRates.find(rateKey);
        if (contextIt != mContextExchangeRates.end()) {
            actualExchangeRate = contextIt->second;
        } else if (equivalentsDiffer) {
            auto rateFromManager = mExchangeRatesManager->get(
                incomingEquivalent,
                outgoingEquivalent);
            if (rateFromManager != nullptr) {
                mContextExchangeRates.emplace(rateKey, rateFromManager);
                actualExchangeRate = rateFromManager;
            }
        }

        if (!actualExchangeRate.has_value()) {
            info() << "Exchange rate expected but not available for equivalents "
                   << incomingEquivalent << " -> " << outgoingEquivalent;
            return sendRejectedDueConditionsChanged(
                       std::nullopt,
                       std::nullopt);
        }

        const auto &expectedRate = expectedExchangeRate.value();
        const auto &currentRate = actualExchangeRate.value();
        if (currentRate->exchangeRate() != expectedRate.first ||
            currentRate->exchangeRateShift() != expectedRate.second) {
            info() << "Exchange rate mismatch on path " << kReservation.pathID
                   << ": expected rate=" << expectedRate.first
                   << " shift=" << expectedRate.second
                   << ", actual rate=" << currentRate->exchangeRate()
                   << " shift=" << currentRate->exchangeRateShift();
            return sendRejectedDueConditionsChanged(
                       actualExchangeRate,
                       std::nullopt);
        }
    }

    if (expectedCommission.has_value()) {
        auto contextIt = mContextCommissions.find(incomingEquivalent);
        if (contextIt != mContextCommissions.end()) {
            actualCommission = contextIt->second;
        } else {
            auto commissionFromManager = mCommissionsManager->getCommission(incomingEquivalent);
            if (commissionFromManager != nullptr) {
                mContextCommissions.emplace(
                    incomingEquivalent,
                    commissionFromManager);
                actualCommission = commissionFromManager;
            }
        }

        if (!actualCommission.has_value()) {
            info() << "Commission expected but not available for equivalent "
                   << incomingEquivalent;
            return sendRejectedDueConditionsChanged(
                       std::nullopt,
                       TrustLineAmount(0));
        }

        const auto expectedCommissionAmount = expectedCommission.value();
        const auto actualCommissionAmount = actualCommission.value()->amount();
        if (actualCommissionAmount != expectedCommissionAmount) {
            info() << "Commission mismatch on path " << kReservation.pathID
                   << ": expected=" << expectedCommissionAmount
                   << ", actual=" << actualCommissionAmount;
            return sendRejectedDueConditionsChanged(
                       std::nullopt,
                       TrustLineAmount(actualCommissionAmount));
        }
    }

    if (!expectedExchangeRate.has_value() && equivalentsDiffer) {
        ExchangeRate::Shared availableRate = nullptr;
        auto contextIt = mContextExchangeRates.find(rateKey);
        if (contextIt != mContextExchangeRates.end()) {
            availableRate = contextIt->second;
        } else {
            auto rateFromManager = mExchangeRatesManager->get(
                incomingEquivalent,
                outgoingEquivalent);
            if (rateFromManager != nullptr) {
                mContextExchangeRates.emplace(rateKey, rateFromManager);
                availableRate = rateFromManager;
            }
        }

        if (availableRate != nullptr) {
            info() << "Exchange rate exists on node but was not provided in request";
            return sendRejectedDueConditionsChanged(
                       availableRate,
                       std::nullopt);
        }
    }

    if (!expectedCommission.has_value() && !commissionAlreadyCharged) {
        Commission::Shared availableCommission = nullptr;
        auto contextIt = mContextCommissions.find(incomingEquivalent);
        if (contextIt != mContextCommissions.end()) {
            availableCommission = contextIt->second;
        } else {
            auto commissionFromManager = mCommissionsManager->getCommission(incomingEquivalent);
            if (commissionFromManager != nullptr) {
                mContextCommissions.emplace(
                    incomingEquivalent,
                    commissionFromManager);
                availableCommission = commissionFromManager;
            }
        }

        if (availableCommission != nullptr &&
            availableCommission->amount() > TrustLineAmount(0)) {
            info() << "Commission exists on node but was not provided in request";
            return sendRejectedDueConditionsChanged(
                       std::nullopt,
                       TrustLineAmount(availableCommission->amount()));
        }
    }

    // Validate outgoing reservation against incoming reservation according to current conditions
    bool reservationValid = true;
    if (!equivalentsDiffer) {
        auto commissionIt = mContextCommissions.find(incomingEquivalent);
        if (!commissionAlreadyCharged &&
            commissionIt != mContextCommissions.end() &&
            commissionIt->second != nullptr) {
            const TrustLineAmount commissionAmount = commissionIt->second->amount();
            TrustLineAmount expectedOutgoing = TrustLine::kZeroAmount();
            if (incomingAmount >= commissionAmount) {
                expectedOutgoing = incomingAmount - commissionAmount;
            }

            if (requestedOutgoingAmount != expectedOutgoing) {
                warning() << "Outgoing amount " << requestedOutgoingAmount
                          << " does not match incoming amount " << incomingAmount
                          << " minus commission " << commissionAmount
                          << " on path " << kReservation.pathID;
                reservationValid = false;
            }
        } else {
            if (requestedOutgoingAmount != incomingAmount) {
                warning() << "Outgoing amount " << requestedOutgoingAmount
                          << " does not match incoming amount " << incomingAmount
                          << " on path " << kReservation.pathID;
                reservationValid = false;
            }
        }
    } else {
        auto rateIt = mContextExchangeRates.find(rateKey);
        if (rateIt == mContextExchangeRates.end() || rateIt->second == nullptr) {
            error() << "Exchange rate missing in context for path " << kReservation.pathID;
            reservationValid = false;
        } else {
            const auto &rate = rateIt->second;
            if (rate->equivalentFrom() != incomingEquivalent ||
                rate->equivalentTo() != outgoingEquivalent) {
                warning() << "Exchange rate equivalents mismatch for path " << kReservation.pathID;
            }

            try {
                const auto expectedOutgoing = ExchangeRateMath::applyExchangeRate(
                    incomingAmount,
                    rate->exchangeRate(),
                    rate->exchangeRateShift());

                if (requestedOutgoingAmount != expectedOutgoing) {
                    warning() << "Outgoing amount " << requestedOutgoingAmount
                              << " does not match exchange conversion result "
                              << expectedOutgoing << " on path " << kReservation.pathID;
                    reservationValid = false;
                }
            } catch (const std::exception &e) {
                error() << "Failed to calculate expected outgoing amount: " << e.what();
                reservationValid = false;
            }
        }
    }

    if (!reservationValid) {
        return sendErrorMessageOnCoordinatorRequest(
                   ResponseMessage::Rejected);
    }

    const auto kOutgoingAmount = trustLines->outgoingTrustAmountConsideringReservations(nextNodeID);
    debug() << "available outgoing amount to next node is " << *kOutgoingAmount;
    TrustLineAmount reservationAmount = std::min(
                                            *kReservation.amount.get(),
                                            *kOutgoingAmount);

    if (reservationAmount == TrustLineAmount(0) ||
            !reserveOutgoingAmount(
                nextNodeID,
                reservationAmount,
                kReservation.pathID,
                kReservation.equivalent)) {
        warning() << "No amount reservation is possible. Rolled back.";
        return sendErrorMessageOnCoordinatorRequest(
                   ResponseMessage::Rejected);
    }

#ifdef TESTS
    mSubsystemsController->testForbidSendRequestToIntNodeOnReservationStage(
        kNextNode,
        reservationAmount);
#endif

    debug() << "Reserve locally " << reservationAmount << " to node "
            << kNextNode->fullAddress() << " on path " << kReservation.pathID;
    mLastReservedAmount = reservationAmount;

    vector<PathReservation> reservations;
    reservations.emplace_back(
        kReservation.pathID,
        make_shared<const TrustLineAmount>(reservationAmount),
        kReservation.equivalent,
        PathReservation::Outgoing);

    if (kFinalAmounts.size() > 1) {
        reservations.insert(
            reservations.end(),
            kFinalAmounts.begin() + 1,
            kFinalAmounts.end());
    }
    debug() << "Prepared for sending reservations size: " << reservations.size();

    sendMessage<IntermediateNodeReservationRequestMessage>(
        nextNodeID,
        mEquivalent,
        mContractorsManager->idOnContractorSide(nextNodeID),
        currentTransactionUUID(),
        reservations);

    mStep = Stages::IntermediateNode_NextNeighborResponseProcessing;
    return resultWaitForMessageTypes( {
        Message::Payments_IntermediateNodeReservationResponse,
        Message::Payments_IntermediateNodeReservationRequest,
        Message::Payments_FinalPathExchangeConfiguration,
        Message::Payments_FinalAmountsConfiguration,
        Message::Payments_TransactionPublicKeyHash,
        Message::Payments_TTLProlongationResponse,
        Message::General_NoEquivalent},
    maxNetworkDelay(2));
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::runNextNeighborResponseProcessingStage()
{
    debug() << "runNextNeighborResponseProcessingStage";
    if (mContext.empty()) {
        warning() << "No amount reservation response received. Rolled back.";
        return sendErrorMessageOnNextNodeResponse(
                   ResponseMessage::NextNodeInaccessible);
    }

    if (contextIsValid(Message::General_NoEquivalent, false)) {
        warning() << "Neighbor hasn't TLs on requested equivalent. Rolled back.";
        return sendErrorMessageOnNextNodeResponse(
                   ResponseMessage::Rejected);
    }

    if (!contextIsValid(Message::Payments_IntermediateNodeReservationResponse)) {
        return runReservationProlongationStage();
    }

    const auto kMessage = popNextMessage<IntermediateNodeReservationResponseMessage>();
    auto nextNodeAddress = kMessage->senderAddresses.at(0);
    info() << "Next node " << nextNodeAddress->fullAddress() << " sent response";

    auto senderID = mContractorsManager->contractorIDByAddress(nextNodeAddress);
    if (senderID == ContractorsManager::kNotFoundContractorID) {
        warning() << "Sender node is not a neighbor";
        return sendErrorMessageOnNextNodeResponse(
                   ResponseMessage::Rejected);
    }
    info() << "Sender ID " << senderID;

    auto *const baseTrustLines = trustLinesManager(kMessage->equivalent());

#ifdef TESTS
    mSubsystemsController->testForbidSendMessageToCoordinatorOnReservationStage(
        nextNodeAddress,
        kMessage->amountReserved());
    mSubsystemsController->testThrowExceptionOnNextNeighborResponseProcessingStage();
    mSubsystemsController->testTerminateProcessOnNextNeighborResponseProcessingStage();
#endif

    if (kMessage->pathID() != mLastProcessedPath) {
        warning() << "Next node sent response on wrong path " << kMessage->pathID()
                  << " . Continue previous state";
        return resultContinuePreviousState();
    }

    if (kMessage->state() == IntermediateNodeReservationResponseMessage::Closed) {
        warning() << "Amount reservation rejected with further transaction closing by the Receiver node.";
        return sendErrorMessageOnNextNodeResponse(
                   ResponseMessage::Closed);
    }

    if (kMessage->state() == IntermediateNodeReservationResponseMessage::Rejected) {
        warning() << "Amount reservation rejected by the neighbor node.";
        return sendErrorMessageOnNextNodeResponse(
                   ResponseMessage::Rejected);
    }

    if (kMessage->state() == IntermediateNodeReservationResponseMessage::RejectedDueAuditPending) {
        warning() << "Neighbor node doesn't approved reservation request due to audit pending";
        return sendErrorMessageOnNextNodeResponse(
                   ResponseMessage::RejectedDueAuditPending);
    }

    if (kMessage->state() == IntermediateNodeReservationResponseMessage::RejectedDueContractorKeysAbsence) {
        warning() << "Neighbor node doesn't approved reservation request due to contractor keys absence";
        publicKeysSharingSignal(senderID, mEquivalent);
        baseTrustLines->setIsOwnKeysPresent(senderID, false);
        return sendErrorMessageOnNextNodeResponse(
                   ResponseMessage::RejectedDueContractorKeysAbsence);
    }

    if (kMessage->state() == IntermediateNodeReservationResponseMessage::RejectedDueOwnKeysAbsence) {
        warning() << "Neighbor node doesn't approved reservation request due to own keys absence";
        baseTrustLines->setIsContractorKeysPresent(senderID, false);
        return sendErrorMessageOnNextNodeResponse(
                   ResponseMessage::RejectedDueContractorKeysAbsence);
    }

    debug() << "Next node accepted amount reservation.";

    const PathID pathID = kMessage->pathID();
    AmountReservation::ConstShared incomingReservation = nullptr;
    SerializedEquivalent incomingEquivalent = 0;

    for (const auto &[contractorID, reservationsMap] : mReservations) {
        for (const auto &[resPathID, reservation] : reservationsMap) {
            if (resPathID == pathID &&
                reservation->direction() == AmountReservation::Incoming) {
                incomingReservation = reservation;
                incomingEquivalent = reservation->equivalent();
                break;
            }
        }
        if (incomingReservation) {
            break;
        }
    }

    if (!incomingReservation) {
        error() << "Incoming reservation not found for pathID " << pathID;
        return sendErrorMessageOnNextNodeResponse(
            ResponseMessage::Rejected);
    }

    const SerializedEquivalent outgoingEquivalent = kMessage->equivalent();

    if (kMessage->amountReserved() != mLastReservedAmount) {
        shortageOutgoingReservationsOnPath(
            pathID,
            outgoingEquivalent,
            kMessage->amountReserved());

        const TrustLineAmount outgoingAmount = kMessage->amountReserved();
        TrustLineAmount incomingAmount;

        if (incomingEquivalent == outgoingEquivalent) {
            auto commissionIt = mContextCommissions.find(incomingEquivalent);
            const bool commissionPending =
                commissionIt != mContextCommissions.end() &&
                commissionIt->second != nullptr &&
                commissionIt->second->amount() > TrustLineAmount(0) &&
                mChargedCommissionEquivalents.find(incomingEquivalent) == mChargedCommissionEquivalents.end();

            if (commissionPending) {
                incomingAmount = outgoingAmount + commissionIt->second->amount();

                debug() << "Calculating incoming reservation with commission: "
                        << "outgoing=" << outgoingAmount
                        << ", commission=" << commissionIt->second->amount()
                        << ", incoming=" << incomingAmount;
            } else {
                incomingAmount = outgoingAmount;

                debug() << "Calculating incoming reservation without commission adjustment: "
                        << "incoming=" << incomingAmount;
            }
        } else {
            auto rateIt = mContextExchangeRates.find(std::make_pair(incomingEquivalent, outgoingEquivalent));
            if (rateIt == mContextExchangeRates.end() || rateIt->second == nullptr) {
                error() << "Exchange rate missing in context for incoming calculation: "
                        << incomingEquivalent << " -> " << outgoingEquivalent;
                return sendErrorMessageOnNextNodeResponse(
                    ResponseMessage::Rejected);
            }

            try {
                incomingAmount = ExchangeRateMath::invertExchangeRate(
                    outgoingAmount,
                    rateIt->second->exchangeRate(),
                    rateIt->second->exchangeRateShift());

                debug() << "Calculating incoming reservation via context exchange rate: "
                        << "outgoing=" << outgoingAmount << " (equiv " << outgoingEquivalent << "), "
                        << "incoming=" << incomingAmount << " (equiv " << incomingEquivalent << "), "
                        << "rate=" << rateIt->second->exchangeRate()
                        << ", shift=" << rateIt->second->exchangeRateShift();
            } catch (const std::exception &e) {
                error() << "Error calculating incoming amount: " << e.what();
                return sendErrorMessageOnNextNodeResponse(
                    ResponseMessage::Rejected);
            }
        }

        shortageIncomingReservationsOnPath(pathID, incomingEquivalent, incomingAmount);

        info() << "Incoming reservation adjusted: "
               << "pathID=" << pathID
               << ", incoming=" << incomingAmount << " (equiv " << incomingEquivalent << ")"
               << ", outgoing=" << outgoingAmount << " (equiv " << outgoingEquivalent << ")";
    }

    mLastReservedAmount = kMessage->amountReserved();

    if (incomingEquivalent == outgoingEquivalent) {
        auto commissionIt = mContextCommissions.find(incomingEquivalent);
        if (commissionIt != mContextCommissions.end() &&
            commissionIt->second != nullptr &&
            commissionIt->second->amount() > TrustLineAmount(0)) {
            mChargedCommissionEquivalents.insert(incomingEquivalent);
        }
    }

#ifdef TESTS
    mSubsystemsController->testSleepOnNextNeighborResponseProcessingStage(
        maxNetworkDelay(3) - 500);
#endif

    sendMessage<CoordinatorReservationResponseMessage>(
        mCoordinator->mainAddress(),
        mEquivalent,
        mContractorsManager->ownAddresses(),
        currentTransactionUUID(),
        kMessage->pathID(),
        ResponseMessage::Accepted,
        mLastReservedAmount);

    mStep = Stages::Common_FinalPathConfigurationChecking;
    return resultWaitForMessageTypes( {
        Message::Payments_FinalPathExchangeConfiguration,
        Message::Payments_IntermediateNodeReservationRequest,
        Message::Payments_FinalAmountsConfiguration,
        Message::Payments_TransactionPublicKeyHash,
        Message::Payments_TTLProlongationResponse},
    maxNetworkDelay((kMaxPathLength - 2) * 4));
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::runFinalPathConfigurationProcessingStage()
{
    debug() << "runFinalPathConfigurationProcessingStage";

    if (!contextIsValid(Message::Payments_FinalPathExchangeConfiguration, false)) {
        warning() << "No final paths configuration was received from the coordinator.";
        return runReservationProlongationStage();
    }

    const auto kMessage = popNextMessage<FinalPathExchangeConfigurationMessage>();

    debug() << "Final payment path exchange configuration received";
    debug() << "PathID: " << kMessage->pathID()
            << " incoming: " << kMessage->incomingAmount() << " (equiv " << kMessage->incomingEquivalent() << ")"
            << " outgoing: " << kMessage->outgoingAmount() << " (equiv " << kMessage->outgoingEquivalent() << ")";

    // Check if this is a drop reservation message (both amounts are zero)
    if (kMessage->incomingAmount() == TrustLine::kZeroAmount() &&
        kMessage->outgoingAmount() == TrustLine::kZeroAmount()) {
        debug() << "Drop reservation message received";
        rollBack(kMessage->pathID());
        clearContextIfTransactionIdle();
        if (canCompleteTransaction()) {
            debug() << "There are no reservations. Transaction closed.";
            return resultDone();
        }
    } else {
        // Normal path configuration - validate and update reservations
        shortageIncomingReservationsOnPath(
            kMessage->pathID(),
            kMessage->incomingEquivalent(),
            kMessage->incomingAmount());

        shortageOutgoingReservationsOnPath(
            kMessage->pathID(),
            kMessage->outgoingEquivalent(),
            kMessage->outgoingAmount());
    }

    mStep = Stages::IntermediateNode_ReservationProlongation;
    return resultWaitForMessageTypes( {
        Message::Payments_FinalAmountsConfiguration,
        Message::Payments_TransactionPublicKeyHash,
        Message::Payments_IntermediateNodeReservationRequest,
        Message::Payments_TTLProlongationResponse},
    maxNetworkDelay((kMaxPathLength - 2) * 4));
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::runReservationProlongationStage()
{
    debug() << "runReservationProlongationStage";
    const auto transactionDuration = utc_now() - mCreationTime;
    if (transactionDuration.total_seconds() > kMaxTransactionDurationSeconds) {
        return reject("Transaction duration time has expired. Rolling back");
    }

    if (contextIsValid(Message::Payments_IntermediateNodeReservationRequest, false)) {
        mMessage = popNextMessage<IntermediateNodeReservationRequestMessage>();
        return runPreviousNeighborRequestProcessingStage();
    }

    if (contextIsValid(Message::Payments_FinalPathExchangeConfiguration, false)) {
        return runFinalPathConfigurationProcessingStage();
    }

    if (contextIsValid(Message::Payments_TransactionPublicKeyHash, false)) {
        mStep = Coordinator_FinalAmountsConfigurationConfirmation;
        return runFinalReservationsNeighborConfirmation();
    }

    if (contextIsValid(Message::Payments_FinalAmountsConfiguration, false)) {
        mTTLRequestWasSend = false;
        mStep = Coordinator_FinalAmountsConfigurationConfirmation;
        return runFinalReservationsCoordinatorConfirmation();
    }

    if (contextIsValid(Message::Payments_TTLProlongationResponse, false)) {
        mTTLRequestWasSend = false;
        const auto kMessage = popNextMessage<TTLProlongationResponseMessage>();
        if (kMessage->state() == TTLProlongationResponseMessage::Continue) {
            info() << "Transactions is still alive. Continue waiting for messages";
            if (utc_now() - mTimeStarted > kMaxTransactionDuration()) {
                return reject("Transaction duration time has expired. Rolling back");
            }
            return resultWaitForMessageTypes( {
                Message::Payments_TTLProlongationResponse,
                Message::Payments_FinalPathExchangeConfiguration,
                Message::Payments_FinalAmountsConfiguration,
                Message::Payments_TransactionPublicKeyHash,
                Message::Payments_IntermediateNodeReservationRequest},
            maxNetworkDelay((kMaxPathLength - 2) * 4));
        }
        return reject("Coordinator send response with transaction finish state. Rolling Back");
    }

    if (mTTLRequestWasSend) {
        return reject("No TTL response message received. Transaction will be closed. Rolling Back");
    }

    if (mCoordinator == nullptr) {
        return reject("Node doesn't know coordinator yet. Transaction will be closed. Rolling Back");
    }

    debug() << "Send TTLTransaction message to coordinator " << mCoordinator->mainAddress()->fullAddress();
    sendMessage<TTLProlongationRequestMessage>(
        mCoordinator->mainAddress(),
        mEquivalent,
        mContractorsManager->ownAddresses(),
        currentTransactionUUID());

    mTTLRequestWasSend = true;
    return resultWaitForMessageTypes( {
        Message::Payments_IntermediateNodeReservationRequest,
        Message::Payments_FinalPathExchangeConfiguration,
        Message::Payments_FinalAmountsConfiguration,
        Message::Payments_TransactionPublicKeyHash,
        Message::Payments_TTLProlongationResponse},
    maxNetworkDelay(2));
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::runFinalAmountsConfigurationConfirmation()
{
    debug() << "runFinalAmountsConfigurationConfirmation";
    if (contextIsValid(Message::Payments_FinalAmountsConfiguration, false)) {
        return runFinalReservationsCoordinatorConfirmation();
    }

    if (contextIsValid(Message::Payments_TransactionPublicKeyHash, false)) {
        return runFinalReservationsNeighborConfirmation();
    }

    if (contextIsValid(Message::Payments_TTLProlongationResponse, false)) {
        debug() << "Receive TTL prolongation message";
        const auto kMessage = popNextMessage<TTLProlongationResponseMessage>();
        if (kMessage->state() == TTLProlongationResponseMessage::Continue) {
            info() << "Transactions is still alive. Continue waiting for messages";
            if (utc_now() - mTimeStarted > kMaxTransactionDuration()) {
                return reject("Transaction duration time has expired. Rolling back");
            }
            return resultWaitForMessageTypes( {
                Message::Payments_TTLProlongationResponse,
                Message::Payments_FinalAmountsConfiguration,
                Message::Payments_TransactionPublicKeyHash},
            maxNetworkDelay((kMaxPathLength - 1) * 4));
        }
        removeAllDataFromStorageConcerningTransaction();
        return reject("Coordinator send TTL message with transaction finish state. Rolling Back");
    }

    if (mTTLRequestWasSend) {
        removeAllDataFromStorageConcerningTransaction();
        return reject("No all final amounts configuration received. Transaction will be closed. Rolling Back");
    }

    debug() << "Send TTLTransaction message to coordinator " << mCoordinator->mainAddress()->fullAddress();
    sendMessage<TTLProlongationRequestMessage>(
        mCoordinator->mainAddress(),
        mEquivalent,
        mContractorsManager->ownAddresses(),
        currentTransactionUUID());

    mTTLRequestWasSend = true;
    return resultWaitForMessageTypes( {
        Message::Payments_FinalAmountsConfiguration,
        Message::Payments_TransactionPublicKeyHash,
        Message::Payments_TTLProlongationResponse},
    maxNetworkDelay(2));
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::runFinalReservationsCoordinatorConfirmation()
{
    debug() << "runFinalReservationsCoordinatorConfirmation";

#ifdef TESTS
    mSubsystemsController->testForbidSendMessageOnFinalAmountClarificationStage();
#endif

    auto kMessage = popNextMessage<FinalAmountsConfigurationMessage>();
    auto coordinatorAddress = kMessage->senderAddresses.at(0);

    mMaximalClaimingBlockNumber = kMessage->maximalClaimingBlockNumber();
    debug() << "maximal claiming block number on Coordinator side: " << mMaximalClaimingBlockNumber;
    mDisputeGracePeriodBlocksCount = kMessage->disputeGracePeriodBlocksCount();
    debug() << "dispute grace period blocks count on Coordinator side: " << mDisputeGracePeriodBlocksCount;
    
    info() << "final amount configuration size: " << kMessage->finalAmountsConfiguration().size();

    if (!updateReservations(
                kMessage->finalAmountsConfiguration())) {
        sendErrorMessageOnFinalAmountsConfiguration();
        removeAllDataFromStorageConcerningTransaction();
        return reject("There are some final amounts, reservations for which are absent. Rejected");
    }

#ifdef TESTS
    mSubsystemsController->testSleepOnFinalAmountClarificationStage(
        maxNetworkDelay(8));
#endif

    info() << "All reservations were updated";

    if (!checkReservationsDirections()) {
        removeAllDataFromStorageConcerningTransaction();
        return reject("Reservations on node are invalid");
    }
    info() << "All reservations are correct";

    mPaymentParticipants = kMessage->paymentParticipants();
    for (const auto &paymentParticipant : mPaymentParticipants) {
        mPaymentNodesIds.insert(
            make_pair(
                paymentParticipant.second->mainAddress()->fullAddress(),
                paymentParticipant.first));
    }
    if (!checkAllNeighborsWithReservationsAreInFinalParticipantsList()) {
        removeAllDataFromStorageConcerningTransaction();
        sendErrorMessageOnFinalAmountsConfiguration();
        return reject("Node has reservation with participant, which not included in mPaymentNodesIds. Rejected");
    }

    auto ioTransaction = mStorageHandler->beginTransaction();
    if (kMessage->isReceiptContains()) {
        info() << "Coordinator send " << kMessage->signatures().size() << " receipt(s)";
        auto coordinatorID = mContractorsManager->contractorIDByAddress(coordinatorAddress);
        if (coordinatorID == ContractorsManager::kNotFoundContractorID) {
            removeAllDataFromStorageConcerningTransaction(ioTransaction);
            sendErrorMessageOnFinalAmountsConfiguration();
            return reject("Coordinator is not a neighbor. Rejected");
        }

        // Group incoming reservations by equivalent
        map<SerializedEquivalent, TrustLineAmount> incomingAmountsByEquivalent;
        if (mReservations.find(coordinatorID) != mReservations.end()) {
            for (const auto &pathIDAndReservation : mReservations[coordinatorID]) {
                if (pathIDAndReservation.second->direction() == AmountReservation::Incoming) {
                    SerializedEquivalent equiv = pathIDAndReservation.second->equivalent();
                    if (incomingAmountsByEquivalent.find(equiv) == incomingAmountsByEquivalent.end()) {
                        incomingAmountsByEquivalent[equiv] = TrustLine::kZeroAmount();
                    }
                    incomingAmountsByEquivalent[equiv] = incomingAmountsByEquivalent[equiv] + pathIDAndReservation.second->amount();
                }
            }
        }

        // Verify each receipt
        for (const auto &[equivalent, signature] : kMessage->signatures()) {
            auto trustLines = trustLinesManager(equivalent);
            auto coordinatorTotalIncomingReservationAmount = incomingAmountsByEquivalent[equivalent];

            auto keyChain = mKeysStore->keychain(trustLines->trustLineID(coordinatorID));
            // Receipt is addressed to current node; bind it to own payment public key.
            mKeysStore->ensurePaymentKeyExists(ioTransaction);
            auto recipientPaymentPublicKey = ioTransaction->paymentKeysHandler()->getOwnPublicKey();
            if (recipientPaymentPublicKey == nullptr) {
                removeAllDataFromStorageConcerningTransaction(ioTransaction);
                sendErrorMessageOnFinalAmountsConfiguration();
                return reject("Own payment public key is absent. Rejected");
            }
            auto serializedIncomingReceiptData = getSerializedReceipt(
                    recipientPaymentPublicKey,
                    coordinatorTotalIncomingReservationAmount,
                    equivalent);
            if (!keyChain.checkSign(
                        ioTransaction,
                        serializedIncomingReceiptData.first,
                        serializedIncomingReceiptData.second,
                        signature)) {
                removeAllDataFromStorageConcerningTransaction(ioTransaction);
                sendErrorMessageOnFinalAmountsConfiguration();
                warning() << "Coordinator send invalid receipt signature for equivalent " << equivalent;
                return reject("Coordinator send invalid receipt signature. Rejected");
            }
            if (!keyChain.saveIncomingPaymentReceipt(
                        ioTransaction,
                        mTransactionUUID,
                        coordinatorTotalIncomingReservationAmount,
                        signature)) {
                removeAllDataFromStorageConcerningTransaction(ioTransaction);
                sendErrorMessageOnFinalAmountsConfiguration();
                warning() << "Can't save coordinator receipt for equivalent " << equivalent;
                return reject("Can't save coordinator receipt. Rejected.");
            }
            debug() << "Verified and saved receipt for equivalent " << equivalent
                    << " with amount " << coordinatorTotalIncomingReservationAmount;
        }
        info() << "All coordinator's receipts are valid";
    } else {
        auto coordinatorID = mContractorsManager->contractorIDByAddress(coordinatorAddress);
        if (coordinatorID != ContractorsManager::kNotFoundContractorID) {
            // Check all equivalents have zero incoming reservations
            if (mReservations.find(coordinatorID) != mReservations.end()) {
                for (const auto &pathIDAndReservation : mReservations[coordinatorID]) {
                    if (pathIDAndReservation.second->direction() == AmountReservation::Incoming &&
                        pathIDAndReservation.second->amount() != TrustLine::kZeroAmount()) {
                        removeAllDataFromStorageConcerningTransaction(ioTransaction);
                        sendErrorMessageOnFinalAmountsConfiguration();
                        warning() << "Receipt amount: 0. Local incoming amount for equivalent "
                                  << pathIDAndReservation.second->equivalent() << ": "
                                  << pathIDAndReservation.second->amount();
                        return reject("Coordinator send invalid receipt amount. Rejected");
                    }
                }
            }
        }
    }

    mStep = Common_ObservingBlockNumberProcessing;
    mBlockNumberObtainingInProcess = true;
    sendRpcRequest(
        make_shared<GetBlockNumberRpcRequest>(
            mTransactionUUID));
    return resultWaitForRpcResponse(
        RpcMethod::GetBlockNumber,
        maxNetworkDelay(1));
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::runFinalReservationsNeighborConfirmation()
{
    debug() << "runFinalReservationsNeighborConfirmation";
    auto kMessage = popNextMessage<TransactionPublicKeyHashMessage>();
    auto senderAddress = kMessage->senderAddresses.at(0);
    debug() << "sender: " << senderAddress->fullAddress();

    mParticipantsPublicKeysHashes[senderAddress->fullAddress()] = make_pair(
            kMessage->paymentNodeID(),
            kMessage->transactionPublicKeyHash());

    auto ioTransaction = mStorageHandler->beginTransaction();
    if (kMessage->isReceiptContains()) {
        info() << "Sender send " << kMessage->signatures().size() << " receipt(s)";
        auto senderID = mContractorsManager->contractorIDByAddress(senderAddress);
        if (senderID == ContractorsManager::kNotFoundContractorID) {
            removeAllDataFromStorageConcerningTransaction(ioTransaction);
            sendErrorMessageOnFinalAmountsConfiguration();
            return reject("Sender is not a neighbor. Rejected");
        }

        // Group incoming reservations by equivalent
        map<SerializedEquivalent, TrustLineAmount> incomingAmountsByEquivalent;
        if (mReservations.find(senderID) != mReservations.end()) {
            for (const auto &pathIDAndReservation : mReservations[senderID]) {
                if (pathIDAndReservation.second->direction() == AmountReservation::Incoming) {
                    SerializedEquivalent equiv = pathIDAndReservation.second->equivalent();
                    if (incomingAmountsByEquivalent.find(equiv) == incomingAmountsByEquivalent.end()) {
                        incomingAmountsByEquivalent[equiv] = TrustLine::kZeroAmount();
                    }
                    incomingAmountsByEquivalent[equiv] = incomingAmountsByEquivalent[equiv] + pathIDAndReservation.second->amount();
                }
            }
        }

        // Verify each receipt
        for (const auto &[equivalent, signature] : kMessage->signatures()) {
            auto trustLines = trustLinesManager(equivalent);
            auto participantTotalIncomingReservationAmount = incomingAmountsByEquivalent[equivalent];

            auto keyChain = mKeysStore->keychain(trustLines->trustLineID(senderID));
            // Receipt is addressed to current node; bind it to own payment public key.
            mKeysStore->ensurePaymentKeyExists(ioTransaction);
            auto recipientPaymentPublicKey = ioTransaction->paymentKeysHandler()->getOwnPublicKey();
            if (recipientPaymentPublicKey == nullptr) {
                removeAllDataFromStorageConcerningTransaction(ioTransaction);
                sendErrorMessageOnFinalAmountsConfiguration();
                return reject("Own payment public key is absent. Rejected");
            }
            auto serializedIncomingReceiptData = getSerializedReceipt(
                    recipientPaymentPublicKey,
                    participantTotalIncomingReservationAmount,
                    equivalent);
            if (!keyChain.checkSign(
                        ioTransaction,
                        serializedIncomingReceiptData.first,
                        serializedIncomingReceiptData.second,
                        signature)) {
                removeAllDataFromStorageConcerningTransaction(ioTransaction);
                sendErrorMessageOnFinalAmountsConfiguration();
                warning() << "Sender send invalid receipt signature for equivalent " << equivalent;
                return reject("Sender send invalid receipt signature. Rejected");
            }
            if (!keyChain.saveIncomingPaymentReceipt(
                        ioTransaction,
                        mTransactionUUID,
                        participantTotalIncomingReservationAmount,
                        signature)) {
                removeAllDataFromStorageConcerningTransaction(ioTransaction);
                sendErrorMessageOnFinalAmountsConfiguration();
                warning() << "Can't save participant receipt for equivalent " << equivalent;
                return reject("Can't save participant receipt. Rejected.");
            }
            debug() << "Verified and saved receipt for equivalent " << equivalent
                    << " with amount " << participantTotalIncomingReservationAmount;
        }
        info() << "All sender's receipts are valid";
    } else {
        auto senderID = mContractorsManager->contractorIDByAddress(senderAddress);
        if (senderID != ContractorsManager::kNotFoundContractorID) {
            // Check all equivalents have zero incoming reservations
            if (mReservations.find(senderID) != mReservations.end()) {
                for (const auto &pathIDAndReservation : mReservations[senderID]) {
                    if (pathIDAndReservation.second->direction() == AmountReservation::Incoming &&
                        pathIDAndReservation.second->amount() != TrustLine::kZeroAmount()) {
                        removeAllDataFromStorageConcerningTransaction(ioTransaction);
                        sendErrorMessageOnFinalAmountsConfiguration();
                        warning() << "Receipt amount: 0. Local reserved incoming amount for equivalent "
                                  << pathIDAndReservation.second->equivalent() << ": "
                                  << pathIDAndReservation.second->amount();
                        return reject("Sender send invalid receipt amount. Rejected");
                    }
                }
            }
        }
    }

    if (mPaymentParticipants.empty()) {
        return resultWaitForMessageTypes( {
            Message::Payments_FinalAmountsConfiguration,
            Message::Payments_TransactionPublicKeyHash,
            Message::Payments_TTLProlongationResponse},
        maxNetworkDelay(2));
    }

    if (mBlockNumberObtainingInProcess) {
        return resultWaitForRpcResponseAndMessagesTypes(
        RpcMethod::GetBlockNumber,
        {Message::Payments_TransactionPublicKeyHash},
        maxNetworkDelay(1));
    }

    if (mPaymentParticipants.size() == mParticipantsPublicKeysHashes.size() + 1) {
        if (!checkAllPublicKeyHashesProperly()) {
            removeAllDataFromStorageConcerningTransaction(ioTransaction);
            sendErrorMessageOnFinalAmountsConfiguration();
            return reject("Public key hashes are not properly. Rejected");
        }

        sendMessage<FinalAmountsConfigurationResponseMessage>(
            mCoordinator->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            FinalAmountsConfigurationResponseMessage::Accepted,
            mPublicKey);

        info() << "Accepted final amounts configuration";

        mStep = Common_Voting;
        mTTLRequestWasSend = false;
        return resultWaitForMessageTypes( {
            Message::Payments_ParticipantsPublicKeys,
            Message::Payments_TTLProlongationResponse},
        maxNetworkDelay(5)); // todo : need discuss this parameter (5)
    }

    mStep = Coordinator_FinalAmountsConfigurationConfirmation;
    return resultWaitForMessageTypes( {
        Message::Payments_TransactionPublicKeyHash,
        Message::Payments_TTLProlongationResponse},
    maxNetworkDelay(2));
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::runCheckObservingBlockNumber()
{
    info() << "runCheckObservingBlockNumber";
    if (contextIsValid(Message::Payments_TransactionPublicKeyHash, false)) {
        return runFinalReservationsNeighborConfirmation();
    }

    if (contextIsValid(Message::Payments_TTLProlongationResponse, false)) {
        debug() << "Receive TTL prolongation message";
        const auto kMessage = popNextMessage<TTLProlongationResponseMessage>();
        if (kMessage->state() == TTLProlongationResponseMessage::Continue) {
            info() << "Transactions is still alive. Continue waiting for messages";
            if (utc_now() - mTimeStarted > kMaxTransactionDuration()) {
                return reject("Transaction duration time has expired. Rolling back");
            }
            return resultWaitForResourceAndMessagesTypes(
            {BaseResource::ObservingBlockNumber}, {
                Message::Payments_TransactionPublicKeyHash,
                Message::Payments_TTLProlongationResponse
            },
            maxNetworkDelay(1));
        }
        removeAllDataFromStorageConcerningTransaction();
        return reject("Coordinator send TTL message with transaction finish state. Rolling Back");
    }

    if (!mIsSuspendedOnFinalAmountsConfirmationStage) {
        if (!rpcRequestIsValid(RpcMethod::GetBlockNumber)) {
            removeAllDataFromStorageConcerningTransaction();
            sendErrorMessageOnFinalAmountsConfiguration();
            return reject("Can't check observing actual block number. Rejected.");
        }
        auto blockNumberRpcResponse = popRpcResponse<GetBlockNumberRpcResponse>();
        if (blockNumberRpcResponse->status() != RpcResponseStatus::Success) {
            removeAllDataFromStorageConcerningTransaction();
            sendErrorMessageOnFinalAmountsConfiguration();
            return reject("Can't check observing actual block number. RCP response is not successful. Rejected.");
        }
        auto maximalClaimingBlockNumber = blockNumberRpcResponse->blockNumber() + kCountBlocksForClaiming;
        debug() << "maximal claiming block number on own side: " << maximalClaimingBlockNumber;
        if (!checkMaxClaimingBlockNumber(maximalClaimingBlockNumber)) {
            debug() << "Max claiming block number sending by coordinator is invalid: " << mMaximalClaimingBlockNumber;
            removeAllDataFromStorageConcerningTransaction();
            sendErrorMessageOnFinalAmountsConfiguration();
            return reject("Max claiming block number sending by coordinator is invalid. Rejected.");
        }

        if (mDisputeGracePeriodBlocksCount != kDisputeGracePeriodBlocksCount) {
            removeAllDataFromStorageConcerningTransaction();
            sendErrorMessageOnFinalAmountsConfiguration();
            return reject("Dispute grace period blocks count is not properly. Rejected.");
        }
        mBlockNumberObtainingInProcess = false;
    }

    for (const auto &reservation : mReservations) {
        for (const auto &pathIDAndReservation : reservation.second) {
            if (trustLinesManager(pathIDAndReservation.second->equivalent())->trustLineState(reservation.first) == TrustLine::KeysSharing) {
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

    auto ioTransaction = mStorageHandler->beginTransaction();
    mKeysStore->ensurePaymentKeyExists(ioTransaction);
    mPublicKey = ioTransaction->paymentKeysHandler()->getOwnPublicKey();
    mParticipantsPublicKeysHashes.insert(
        make_pair(
            mContractorsManager->selfContractor()->mainAddress()->fullAddress(),
            make_pair(
                mPaymentNodesIds[mContractorsManager->selfContractor()->mainAddress()->fullAddress()],
                mPublicKey->hash())));

    auto ownPaymentID = mPaymentNodesIds[mContractorsManager->selfContractor()->mainAddress()->fullAddress()];
    for (const auto &paymentNodeIdAndContractor : mPaymentParticipants) {
        if (paymentNodeIdAndContractor.second == mCoordinator) {
            continue;
        }
        if (paymentNodeIdAndContractor.second == mContractorsManager->selfContractor()) {
            continue;
        }

        auto participantID = mContractorsManager->contractorIDByAddress(paymentNodeIdAndContractor.second->mainAddress());
        if (mReservations.find(participantID) == mReservations.end()) {
            info() << "Send public key hash to " << paymentNodeIdAndContractor.second->mainAddress()->fullAddress();
            sendMessage<TransactionPublicKeyHashMessage>(
                paymentNodeIdAndContractor.second->mainAddress(),
                mEquivalent,
                mContractorsManager->ownAddresses(),
                currentTransactionUUID(),
                ownPaymentID,
                mPublicKey->hash());
            continue;
        }

        auto nodeReservations = mReservations[participantID];
        if (nodeReservations.begin()->second->direction() == AmountReservation::Outgoing) {
            // Group outgoing reservations by equivalent
            map<SerializedEquivalent, TrustLineAmount> amountsByEquivalent;
            for (const auto &pathIDAndReservation : nodeReservations) {
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
            // Receipt is addressed to participant; bind it to participant payment public key.
            auto recipientPaymentPublicKey = mContractorsManager->contractor(participantID)->paymentPublicKey();
            if (recipientPaymentPublicKey == nullptr) {
                removeAllDataFromStorageConcerningTransaction(ioTransaction);
                sendErrorMessageOnFinalAmountsConfiguration();
                return reject("Participant payment public key is absent. Rejected");
            }
            for (const auto &[equivalent, amount] : amountsByEquivalent) {
                auto trustLines = trustLinesManager(equivalent);
                auto keyChain = mKeysStore->keychain(trustLines->trustLineID(participantID));

                auto serializedOutgoingReceiptData = getSerializedReceipt(
                        recipientPaymentPublicKey,
                        amount,
                        equivalent);
                auto signature = keyChain.sign(
                                                 ioTransaction,
                                                 serializedOutgoingReceiptData.first,
                                                 serializedOutgoingReceiptData.second);
                if (!keyChain.saveOutgoingPaymentReceipt(
                            ioTransaction,
                            mTransactionUUID,
                            amount,
                            signature)) {
                    removeAllDataFromStorageConcerningTransaction(ioTransaction);
                    sendErrorMessageOnFinalAmountsConfiguration();
                    return reject("Can't save outgoing receipt. Rejected.");
                }
                signatures.emplace_back(equivalent, signature);
                debug() << "Created receipt for equivalent " << equivalent << " with amount " << amount;
            }

            info() << "Send public key hash to " << paymentNodeIdAndContractor.second->mainAddress()->fullAddress()
                   << " with " << signatures.size() << " receipt(s)";
            sendMessage<TransactionPublicKeyHashMessage>(
                paymentNodeIdAndContractor.second->mainAddress(),
                mEquivalent,
                mContractorsManager->ownAddresses(),
                currentTransactionUUID(),
                ownPaymentID,
                mPublicKey->hash(),
                signatures);
        } else {
            info() << "Send public key hash to " << paymentNodeIdAndContractor.second->mainAddress()->fullAddress();
            sendMessage<TransactionPublicKeyHashMessage>(
                paymentNodeIdAndContractor.second->mainAddress(),
                mEquivalent,
                mContractorsManager->ownAddresses(),
                currentTransactionUUID(),
                ownPaymentID,
                mPublicKey->hash());
        }
    }

    if (mPaymentParticipants.size() == mParticipantsPublicKeysHashes.size() + 1) {
        info() << "All neighbors send theirs reservations";
        if (!checkAllPublicKeyHashesProperly()) {
            removeAllDataFromStorageConcerningTransaction(ioTransaction);
            sendErrorMessageOnFinalAmountsConfiguration();
            return reject("Public key hashes are not properly. Rejected");
        }
        info() << "All public key hashes are properly";

        sendMessage<FinalAmountsConfigurationResponseMessage>(
            mCoordinator->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            FinalAmountsConfigurationResponseMessage::Accepted,
            mPublicKey);
        info() << "Accepted final amounts configuration";

        mStep = Common_Voting;
        mTTLRequestWasSend = false;
        return resultWaitForMessageTypes( {
            Message::Payments_ParticipantsPublicKeys,
            Message::Payments_TTLProlongationResponse},
        maxNetworkDelay(5));
    }

    mStep = Coordinator_FinalAmountsConfigurationConfirmation;
    return resultWaitForMessageTypes( {
        Message::Payments_TransactionPublicKeyHash,
        Message::Payments_TTLProlongationResponse},
    maxNetworkDelay(2));
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::runVotesCheckingStageWithCoordinatorClarification()
{
    debug() << "runVotesCheckingStageWithCoordinatorClarification";
    if (contextIsValid(Message::Payments_TTLProlongationResponse, false)) {
        debug() << "Receive TTL Message";
        const auto kMessage = popNextMessage<TTLProlongationResponseMessage>();
        if (kMessage->state() == TTLProlongationResponseMessage::Finish) {
            removeAllDataFromStorageConcerningTransaction();
            return reject("Coordinator send response with transaction finish state. Rolling Back");
        }
        debug() << "Transactions is still alive. Continue waiting for messages";
        if (utc_now() - mTimeStarted > kMaxTransactionDuration()) {
            return reject("Transaction duration time has expired. Rolling back");
        }
        return resultWaitForMessageTypes( {
            Message::Payments_ParticipantsPublicKeys,
            Message::Payments_TTLProlongationResponse},
        maxNetworkDelay((kMaxPathLength - 2) * 4));
    }

    if (contextIsValid(Message::Payments_ParticipantsPublicKeys, false)) {
        return runVotesCheckingStage();
    }

    if (mTTLRequestWasSend) {
        removeAllDataFromStorageConcerningTransaction();
        return reject("No participants votes message received. Transaction will be closed. Rolling Back");
    }

    debug() << "Send TTLTransaction message to coordinator " << mCoordinator->mainAddress()->fullAddress();
    sendMessage<TTLProlongationRequestMessage>(
        mCoordinator->mainAddress(),
        mEquivalent,
        mContractorsManager->ownAddresses(),
        currentTransactionUUID());
    mTTLRequestWasSend = true;
    return resultWaitForMessageTypes( {
        Message::Payments_ParticipantsPublicKeys,
        Message::Payments_TTLProlongationResponse},
    maxNetworkDelay(2));
}

void IntermediateNodeExchangePaymentTransaction::shortageIncomingReservationsOnPath(
    const PathID pathID,
    const SerializedEquivalent equivalent,
    const TrustLineAmount &amount)
{
    for (const auto &nodeAndReservations : mReservations) {
        for (const auto &pathIDAndReservation : nodeAndReservations.second) {
            if (pathIDAndReservation.first == pathID && pathIDAndReservation.second->equivalent() == equivalent 
                && pathIDAndReservation.second->direction() == AmountReservation::Incoming) {
                if (pathIDAndReservation.second->amount() != amount) {
                    shortageReservation(
                        nodeAndReservations.first,
                        pathIDAndReservation.second,
                        amount,
                        pathIDAndReservation.first,
                        pathIDAndReservation.second->equivalent());
                }
            }
        }
    }
}

void IntermediateNodeExchangePaymentTransaction::shortageOutgoingReservationsOnPath(
    const PathID pathID,
    const SerializedEquivalent equivalent,
    const TrustLineAmount &amount)
{
    for (const auto &nodeAndReservations : mReservations) {
        for (const auto &pathIDAndReservation : nodeAndReservations.second) {
            if (pathIDAndReservation.first == pathID && pathIDAndReservation.second->equivalent() == equivalent 
                && pathIDAndReservation.second->direction() == AmountReservation::Outgoing) {
                if (pathIDAndReservation.second->amount() != amount) {
                    shortageReservation(
                        nodeAndReservations.first,
                        pathIDAndReservation.second,
                        amount,
                        pathIDAndReservation.first,
                        pathIDAndReservation.second->equivalent());
                }
            }
        }
    }
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::approve()
{
    mCommittedAmount = totalReservedAmount(
                           AmountReservation::Outgoing,
                           mEquivalent);
    BaseExchangePaymentTransaction::approve();
    BaseExchangePaymentTransaction::runThreeNodesCyclesTransactions();
    BaseExchangePaymentTransaction::runFourNodesCyclesTransactions();
    clearContextIfTransactionIdle();
    return resultDone();
}

void IntermediateNodeExchangePaymentTransaction::savePaymentOperationIntoHistory(
    IOTransaction::Shared ioTransaction)
{
    debug() << "savePaymentOperationIntoHistory";
    ioTransaction->historyStorage()->savePaymentAdditionalRecord(
        make_shared<PaymentAdditionalRecord>(
            currentTransactionUUID(),
            PaymentAdditionalRecord::IntermediatePaymentType,
            mCommittedAmount,
            mOutgoingTransfers,
            mIncomingTransfers),
        mEquivalent);
    debug() << "Operation saved";
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::sendErrorMessageOnPreviousNodeRequest(
    BaseAddress::Shared previousNode,
    PathID pathID,
    ResponseMessage::OperationState errorState)
{
    sendMessage<IntermediateNodeReservationResponseMessage>(
        previousNode,
        mEquivalent,
        mContractorsManager->ownAddresses(),
        currentTransactionUUID(),
        pathID,
        errorState);
    if (mReservations.empty()) {
        clearContextIfTransactionIdle();
        if (canCompleteTransaction()) {
            debug() << "There are no reservations. Transaction closed.";
            return resultDone();
        }
    }
    mStep = Stages::IntermediateNode_ReservationProlongation;
    return resultWaitForMessageTypes( {
        Message::Payments_FinalAmountsConfiguration,
        Message::Payments_TransactionPublicKeyHash,
        Message::Payments_IntermediateNodeReservationRequest,
        Message::Payments_TTLProlongationResponse},
    maxNetworkDelay((kMaxPathLength - 2) * 4));
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::sendErrorMessageOnCoordinatorRequest(
    ResponseMessage::OperationState errorState)
{
    sendMessage<CoordinatorReservationResponseMessage>(
        mCoordinator->mainAddress(),
        mEquivalent,
        mContractorsManager->ownAddresses(),
        currentTransactionUUID(),
        mLastProcessedPath,
        errorState);
    rollBack(mLastProcessedPath);
    clearContextIfTransactionIdle();
    if (canCompleteTransaction()) {
        debug() << "There are no reservations. Transaction closed.";
        return resultDone();
    }
    mStep = Stages::IntermediateNode_ReservationProlongation;
    return resultWaitForMessageTypes( {
        Message::Payments_FinalAmountsConfiguration,
        Message::Payments_TransactionPublicKeyHash,
        Message::Payments_IntermediateNodeReservationRequest,
        Message::Payments_TTLProlongationResponse},
    maxNetworkDelay((kMaxPathLength - 2) * 4));
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::sendErrorMessageOnNextNodeResponse(
    ResponseMessage::OperationState errorState)
{
    sendMessage<CoordinatorReservationResponseMessage>(
        mCoordinator->mainAddress(),
        mEquivalent,
        mContractorsManager->ownAddresses(),
        currentTransactionUUID(),
        mLastProcessedPath,
        errorState);
    rollBack(mLastProcessedPath);
    clearContextIfTransactionIdle();
    if (canCompleteTransaction()) {
        debug() << "There are no reservations. Transaction closed.";
        return resultDone();
    }
    mStep = Stages::IntermediateNode_ReservationProlongation;
    return resultWaitForMessageTypes( {
        Message::Payments_FinalAmountsConfiguration,
        Message::Payments_TransactionPublicKeyHash,
        Message::Payments_IntermediateNodeReservationRequest,
        Message::Payments_TTLProlongationResponse},
    maxNetworkDelay((kMaxPathLength - 2) * 4));
}

void IntermediateNodeExchangePaymentTransaction::sendErrorMessageOnFinalAmountsConfiguration()
{
    sendMessage<FinalAmountsConfigurationResponseMessage>(
        mCoordinator->mainAddress(),
        mEquivalent,
        mContractorsManager->ownAddresses(),
        mTransactionUUID,
        FinalAmountsConfigurationResponseMessage::Rejected);
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::sendRejectedDueConditionsChanged(
    const std::optional<ExchangeRate::Shared> &actualExchangeRate,
    const std::optional<TrustLineAmount> &actualCommission)
{
    // Communicate the RejectedDueConditionsChanged status to the coordinator, echoing whichever
    // actual condition (rate or commission) caused the rejection so the coordinator can react.
    if (mCoordinator == nullptr) {
        warning() << "Coordinator is unknown while sending RejectedDueConditionsChanged";
        return resultDone();
    }

    auto senderAddresses = mContractorsManager->ownAddresses();

    CoordinatorReservationResponseMessage::Shared response;
    const bool hasExchangeRate = actualExchangeRate.has_value() && actualExchangeRate.value() != nullptr;
    const bool hasCommission = actualCommission.has_value();

    if (hasExchangeRate && hasCommission) {
        warning() << "Both exchange rate and commission provided for rejection; using exchange rate only";
    }

    if (hasExchangeRate) {
        response = make_shared<CoordinatorReservationResponseMessage>(
            mEquivalent,
            senderAddresses,
            currentTransactionUUID(),
            mLastProcessedPath,
            ResponseMessage::RejectedDueConditionsChanged,
            TrustLine::kZeroAmount(),
            actualExchangeRate.value()->exchangeRate(),
            actualExchangeRate.value()->exchangeRateShift());
    } else if (hasCommission) {
        response = make_shared<CoordinatorReservationResponseMessage>(
            mEquivalent,
            senderAddresses,
            currentTransactionUUID(),
            mLastProcessedPath,
            ResponseMessage::RejectedDueConditionsChanged,
            TrustLine::kZeroAmount(),
            actualCommission.value());
    } else {
        response = make_shared<CoordinatorReservationResponseMessage>(
            mEquivalent,
            senderAddresses,
            currentTransactionUUID(),
            mLastProcessedPath,
            ResponseMessage::RejectedDueConditionsChanged,
            TrustLine::kZeroAmount());
    }

    sendMessage(
        mCoordinator->mainAddress(),
        response);

    rollBack(mLastProcessedPath);
    clearContextIfTransactionIdle();
    if (canCompleteTransaction()) {
        debug() << "There are no reservations. Transaction closed.";
        return resultDone();
    }

    mStep = Stages::IntermediateNode_ReservationProlongation;
    return resultWaitForMessageTypes( {
        Message::Payments_FinalAmountsConfiguration,
        Message::Payments_TransactionPublicKeyHash,
        Message::Payments_IntermediateNodeReservationRequest,
        Message::Payments_TTLProlongationResponse},
    maxNetworkDelay((kMaxPathLength - 2) * 4));
}

bool IntermediateNodeExchangePaymentTransaction::canCompleteTransaction() const
{
    // Completion is possible only when reservations and cached context are both fully cleared.
    return mReservations.empty()
        && mContextExchangeRates.empty()
        && mContextCommissions.empty()
        && mChargedCommissionEquivalents.empty();
}

void IntermediateNodeExchangePaymentTransaction::clearContextIfTransactionIdle()
{
    // Drop cached context once all reservations disappear to prevent stale values on future paths.
    if (!mReservations.empty()) {
        return;
    }
    mContextExchangeRates.clear();
    mContextCommissions.clear();
    mChargedCommissionEquivalents.clear();
}

BaseAddress::Shared IntermediateNodeExchangePaymentTransaction::coordinatorAddress() const
{
    return mCoordinator->mainAddress();
}

bool IntermediateNodeExchangePaymentTransaction::checkReservationsDirections() const
{
    debug() << "checkReservationsDirections";

    // Step 1: Determine outgoing equivalent and validate uniformity
    SerializedEquivalent outgoingEquivalent;
    bool outgoingEquivalentSet = false;
    TrustLineAmount totalOutgoing = TrustLineAmount(0);

    for (const auto& [contractorID, reservations] : mReservations) {
        for (const auto& [pathID, reservation] : reservations) {
            if (reservation->direction() == AmountReservation::Outgoing) {
                if (!outgoingEquivalentSet) {
                    outgoingEquivalent = reservation->equivalent();
                    outgoingEquivalentSet = true;
                } else if (outgoingEquivalent != reservation->equivalent()) {
                    // All outgoing must be in same equivalent
                    warning() << "checkReservationsDirections: Multiple outgoing equivalents detected. "
                              << "First: " << outgoingEquivalent << ", found: " << reservation->equivalent();
                    return false;
                }
                totalOutgoing = totalOutgoing + reservation->amount();
            }
        }
    }

    if (!outgoingEquivalentSet) {
        // No outgoing reservations - invalid for intermediate node
        warning() << "checkReservationsDirections: No outgoing reservations found";
        return false;
    }

    // Step 2: Calculate total incoming converted to outgoing equivalent
    TrustLineAmount totalIncomingConverted = TrustLineAmount(0);
    set<SerializedEquivalent> processedIncomingEquivalents;

    for (const auto& [contractorID, reservations] : mReservations) {
        for (const auto& [pathID, reservation] : reservations) {
            if (reservation->direction() == AmountReservation::Incoming) {
                SerializedEquivalent incomingEquiv = reservation->equivalent();
                TrustLineAmount incomingAmount = reservation->amount();

                if (incomingEquiv == outgoingEquivalent) {
                    // Same equivalent - direct add
                    totalIncomingConverted = totalIncomingConverted + incomingAmount;

                    // Step 3: Deduct commission once per incoming equivalent (transit only)
                    if (processedIncomingEquivalents.find(incomingEquiv) == processedIncomingEquivalents.end()) {
                        auto commission = mCommissionsManager->getCommission(incomingEquiv);
                        if (commission) {
                            TrustLineAmount commissionAmount(commission->amount());
                            totalIncomingConverted = totalIncomingConverted - commissionAmount;
                            debug() << "Deducted commission for equivalent " << incomingEquiv
                                    << ": " << commissionAmount;
                        }
                        processedIncomingEquivalents.insert(incomingEquiv);
                    }
                } else {
                    // Different equivalent - convert
                    auto rate = mExchangeRatesManager->get(incomingEquiv, outgoingEquivalent);
                    if (!rate) {
                        // No exchange rate found
                        warning() << "checkReservationsDirections: No exchange rate found for "
                                  << incomingEquiv << " -> " << outgoingEquivalent;
                        return false;
                    }

                    try {
                        TrustLineAmount converted = mExchangeRatesManager->calculateConvertedAmount(
                            incomingEquiv, outgoingEquivalent, incomingAmount);
                        totalIncomingConverted = totalIncomingConverted + converted;
                        debug() << "Converted " << incomingAmount << " from equiv " << incomingEquiv
                                << " to " << converted << " in equiv " << outgoingEquivalent;
                    } catch (const Exception& e) {
                        // Conversion failed (overflow or other error)
                        warning() << "checkReservationsDirections: Conversion failed: " << e.what();
                        return false;
                    }

                    // No commission for exchange operations
                }
            }
        }
    }

    // Step 4: Compare totals
    debug() << "Total incoming (converted): " << totalIncomingConverted
            << ", total outgoing: " << totalOutgoing;

    return totalIncomingConverted == totalOutgoing;
}

const string IntermediateNodeExchangePaymentTransaction::logHeader() const
{
    stringstream s;
    s << "[IntermediateNodeExchangePaymentTA: " << currentTransactionUUID().stringUUID() << " " << mEquivalent << "] ";
    return s.str();
}
