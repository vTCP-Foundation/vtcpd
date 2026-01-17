#include "ReceiverExchangePaymentTransaction.h"

#include <algorithm>

ReceiverExchangePaymentTransaction::ReceiverExchangePaymentTransaction(
    ReceiverInitPaymentRequestMessage::ConstShared message,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    StorageHandler *storageHandler,
    ResourcesManager *resourcesManager,
    ExchangeRatesManager *exchangeRatesManager,
    CommissionsManager *commissionsManager,
    ExchangePathsManager *exchangePathsManager,
    ObservingHandler *observingHandler,
    Keystore *keystore,
    EventsInterfaceManager *eventsInterfaceManager,
    Logger &log,
    SubsystemsController *subsystemsController) :

    BaseExchangePaymentTransaction(
        BaseTransaction::ReceiverExchangePaymentTransaction,
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
    mExchangeRatesManager(exchangeRatesManager),
    mCommissionsManager(commissionsManager),
    mEventsInterfaceManager(eventsInterfaceManager),
    mCoordinator(make_shared<Contractor>(message->senderAddresses)),
    mTransactionAmount(message->amount()),
    mTransactionShouldBeRejected(false)
{
    mStep = Stages::Receiver_CoordinatorRequestApproving;
    mPayload = message->payload();
}

ReceiverExchangePaymentTransaction::ReceiverExchangePaymentTransaction(
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
    EventsInterfaceManager *eventsInterfaceManager,
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
    mCommissionsManager(commissionsManager),
    mEventsInterfaceManager(eventsInterfaceManager),
    mCoordinator(nullptr),
    mTransactionAmount(0),
    mTransactionShouldBeRejected(false)
{
}

TransactionResult::SharedConst ReceiverExchangePaymentTransaction::run()
{
    try {
        debug() << "mStep: " << mStep;
        switch (mStep) {
        case Stages::Receiver_CoordinatorRequestApproving:
            return runInitializationStage();

        case Stages::Receiver_AmountReservationsProcessing:
            return runAmountReservationStage();

        case Stages::Coordinator_FinalAmountsConfigurationConfirmation:
            return runFinalAmountsConfigurationConfirmation();

        case Stages::Common_ObservingBlockNumberProcessing:
            return runCheckObservingBlockNumber();

        case Stages::Common_Voting:
            return runVotesStageWithCoordinatorClarification();

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
            return resultDone();
        }

        default:
            throw RuntimeError(
                "ReceiverExchangePaymentTransaction::run(): "
                "invalid stage number occurred");
        }
    } catch (Exception &e) {
        warning() << e.what();
        return recover("Something happens wrong in method run(). Transaction will be recovered");
    }
}

TransactionResult::SharedConst ReceiverExchangePaymentTransaction::runInitializationStage()
{
    debug() << "Operation for " << mTransactionAmount << " initialised by the:";
    for (const auto &address : mCoordinator->addresses()) {
        debug() << address->fullAddress();
    }

    auto *const trustLines = trustLinesManager(mEquivalent);

    // Check if total incoming possibilities of the node are <= of the payment amount.
    // If not - there is no reason to process the operation at all.
    // (reject operation)
    const auto kTotalAvailableIncomingAmount = *(trustLines->totalIncomingAmount());
    debug() << "Total incoming amount: " << kTotalAvailableIncomingAmount;
    if (kTotalAvailableIncomingAmount < mTransactionAmount) {
        const auto kTotalIncomingAuditPendingAmount = *(trustLines->totalPossibleIncomingAmountConsiderToAuditPendingTLs());
        info() << "totalPossibleIncomingAmountConsiderToAuditPendingTLs " << kTotalIncomingAuditPendingAmount;
        if (kTotalAvailableIncomingAmount + kTotalIncomingAuditPendingAmount >= mTransactionAmount) {
            info() << "Total incoming possibilities (" << kTotalAvailableIncomingAmount
                   << ") less then operation amount, "
                   << "but there are total incoming audit pending possibilities (" << kTotalIncomingAuditPendingAmount;
            sendMessage<ReceiverInitPaymentResponseMessage>(
                mCoordinator->mainAddress(),
                mEquivalent,
                mContractorsManager->ownAddresses(),
                currentTransactionUUID(),
                ReceiverInitPaymentResponseMessage::RejectedDueAuditPending);
            return resultDone();

        }
        sendMessage<ReceiverInitPaymentResponseMessage>(
            mCoordinator->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            ReceiverInitPaymentResponseMessage::Rejected);

        info() << "Operation rejected due to insufficient funds.";
        return resultDone();
    }

    sendMessage<ReceiverInitPaymentResponseMessage>(
        mCoordinator->mainAddress(),
        mEquivalent,
        mContractorsManager->ownAddresses(),
        currentTransactionUUID(),
        ReceiverInitPaymentResponseMessage::Accepted);

    // Begin waiting for amount reservation requests.
    // There is non-zero probability, that first couple of paths will fail.
    // So receiver will wait for time, that is approximately need for several nodes for processing.
    //
    // TODO: enhancement: send approximate paths count to receiver, so it will be able to wait correct timeout.
    mStep = Stages::Receiver_AmountReservationsProcessing;
    return resultWaitForMessageTypes( {
        Message::Payments_IntermediateNodeReservationRequest,
        Message::Payments_TTLProlongationResponse},
    maxNetworkDelay((kMaxPathLength - 1) * 4));
}

TransactionResult::SharedConst ReceiverExchangePaymentTransaction::runAmountReservationStage()
{
    debug() << "runAmountReservationStage";
    if (contextIsValid(Message::Payments_TTLProlongationResponse, false)) {
        // current path was rejected and need reset delay time
        debug() << "Receive TTL prolongation message";
        const auto kMessage = popNextMessage<TTLProlongationResponseMessage>();
        if (kMessage->state() == TTLProlongationResponseMessage::Continue) {
            debug() << "Transactions is still alive. Continue waiting for messages";
            if (utc_now() - mTimeStarted > kMaxTransactionDuration()) {
                return reject("Transaction duration time has expired. Rolling back");
            }
            return resultWaitForMessageTypes( {
                Message::Payments_IntermediateNodeReservationRequest,
                Message::Payments_TTLProlongationResponse},
            maxNetworkDelay((kMaxPathLength - 1) * 4));
        }
        return reject("Coordinator send TTL message with transaction finish state. Rolling Back");
    }

    if (!contextIsValid(Message::Payments_IntermediateNodeReservationRequest)) {

        if (mTTLRequestWasSend) {
            return reject("No TTL response was received. Transaction will be closed. Rolling Back");
        }

        debug() << "No amount reservation request was received.";
        debug() << "Send TTLTransaction message to coordinator " << mCoordinator->mainAddress()->fullAddress();
        sendMessage<TTLProlongationRequestMessage>(
            mCoordinator->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID());
        mTTLRequestWasSend = true;
        return resultWaitForMessageTypes( {
            Message::Payments_TTLProlongationResponse,
            Message::Payments_IntermediateNodeReservationRequest},
        maxNetworkDelay(2));
    }

    const auto kMessage = popNextMessage<IntermediateNodeReservationRequestMessage>();

    const auto kNeighbor = mContractorsManager->contractorAddresses(kMessage->idOnReceiverSide).at(0);
    const auto &kFinalAmounts = kMessage->finalAmountsConfiguration();
    if (kFinalAmounts.empty()) {
        warning() << "Reservation vector is empty";
        return sendErrorMessageOnPreviousNodeRequest(
                   kNeighbor,
                   0,                  // 0, because we don't know pathID
                   ResponseMessage::Closed);
    }

    const auto &kReservation = kFinalAmounts[0];
    debug() << "Amount reservation for " << *kReservation.amount.get() << " request received from "
            << kNeighbor->fullAddress() << " [" << kReservation.pathID << "]"
            << " equivalent " << kReservation.equivalent;
    debug() << "Received reservations size: " << kFinalAmounts.size();

    auto neighborID = mContractorsManager->contractorIDByAddress(kNeighbor);
    if (neighborID == ContractorsManager::kNotFoundContractorID) {
        warning() << "Previous node is not a neighbor";
        return sendErrorMessageOnPreviousNodeRequest(
                   kNeighbor,
                   kReservation.pathID,
                   ResponseMessage::Rejected);
    }
    debug() << "Sender ID " << neighborID;

    auto *const neighborTrustLines = trustLinesManager(kReservation.equivalent);

    if (!neighborTrustLines->trustLineIsPresent(neighborID)) {
        warning() << "Path is not valid: there is no TL with previous node. Rejected.";
        return sendErrorMessageOnPreviousNodeRequest(
                   kNeighbor,
                   kReservation.pathID,
                   ResponseMessage::Rejected);
    }

    if (!neighborTrustLines->trustLineIsActive(neighborID)) {
        warning() << "Path is not valid: TL with previous node is not active. Rejected.";
        if (neighborTrustLines->trustLineState(neighborID) == TrustLine::AuditPending ||
                neighborTrustLines->trustLineState(neighborID) == TrustLine::KeysSharing) {
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

    if (!neighborTrustLines->trustLineOwnKeysPresent(neighborID)) {
        warning() << "There are no own keys. Rejected";
        publicKeysSharingSignal(neighborID, kReservation.equivalent);
        return sendErrorMessageOnPreviousNodeRequest(
            kNeighbor,
            kReservation.pathID,
            ResponseMessage::RejectedDueOwnKeysAbsence);
    }

    if (!neighborTrustLines->trustLineContractorKeysPresent(neighborID)) {
        warning() << "There are no contractor keys. Rejected";
        return sendErrorMessageOnPreviousNodeRequest(
            kNeighbor,
            kReservation.pathID,
            ResponseMessage::RejectedDueContractorKeysAbsence);
    }

    // update local reservations during amounts from coordinator
    if (!updateReservations(vector<PathReservation>(
                                kFinalAmounts.begin() + 1,
                                kFinalAmounts.end()))) {
        warning() << "Previous node send path configuration, which is absent on current node";
        // next loop is only logger info
        for (const auto &reservation : kFinalAmounts) {
            debug() << "path: " << reservation.pathID << " amount: " << *reservation.amount.get()
                    << " equiv: " << reservation.equivalent;
        }
        mTransactionShouldBeRejected = true;
        return sendErrorMessageOnPreviousNodeRequest(
                   kNeighbor,
                   kReservation.pathID,
                   ResponseMessage::Closed);
    }
    debug() << "All reservations were updated";

    if (!neighborTrustLines->trustLineContractorKeysPresent(neighborID)) {
        warning() << "There are no contractor keys on TL";
        return sendErrorMessageOnPreviousNodeRequest(
                   kNeighbor,
                   kReservation.pathID,
                   ResponseMessage::RejectedDueContractorKeysAbsence);
    }

    // Note: copy of shared pointer is required.
    const auto kAvailableAmount = neighborTrustLines->incomingTrustAmountConsideringReservations(neighborID);
    if (*kAvailableAmount == TrustLine::kZeroAmount()) {
        warning() << "Available amount equals zero. Reservation reject.";
        return sendErrorMessageOnPreviousNodeRequest(
                   kNeighbor,
                   kReservation.pathID,
                   ResponseMessage::Rejected);
    }

    debug() << "Available amount " << *kAvailableAmount;
    const auto kReservationAmount = std::min(
                                        *kReservation.amount.get(),
                                        *kAvailableAmount);

#ifdef TESTS
    if (kNeighbor == mCoordinator->mainAddress()) {
        mSubsystemsController->testForbidSendMessageToCoordinatorOnReservationStage(
            nullptr,
            TrustLine::kZeroAmount());
    }
    mSubsystemsController->testForbidSendResponseToIntNodeOnReservationStage(
        kNeighbor,
        kReservationAmount);
    mSubsystemsController->testThrowExceptionOnPreviousNeighborRequestProcessingStage();
    mSubsystemsController->testTerminateProcessOnPreviousNeighborRequestProcessingStage();
#endif

    if (! reserveIncomingAmount(
            neighborID,
            kReservationAmount,
            kReservation.pathID,
            kReservation.equivalent)) {
        // Receiver must not confirm reservation in case if requested amount is less than available.
        // Intermediate nodes may decrease requested reservation amount, but receiver must not do this.
        // It must stay synchronised with previous node.
        // So, in case if requested amount is less than available -
        // previous node must report about it to the coordinator.
        // In this case, receiver should even not receive reservation request at all.
        //
        // Also, this kind of problem may appear when two nodes are involved
        // in several COUNTER transactions.
        // In this case, balances may be reserved on the nodes,
        // but neighbor node may reject reservation,
        // because it already reserved amount or other transactions,
        // that wasn't approved by the current node yet.
        //
        // In this case, reservation request must be rejected.
        warning() << "Can't reserve incoming amount. Reservation reject.";
        return sendErrorMessageOnPreviousNodeRequest(
                   kNeighbor,
                   kReservation.pathID,
                   ResponseMessage::Rejected);
    }

    const auto kTotalReserved = totalReservedAmount(
                                    AmountReservation::Incoming,
                                    mEquivalent);
    if (kTotalReserved > mTransactionAmount) {
        warning() << "Reserved amount is greater than requested. It indicates protocol or realisation error.";
        mTransactionShouldBeRejected = true;
        return sendErrorMessageOnPreviousNodeRequest(
                   kNeighbor,
                   kReservation.pathID,
                   ResponseMessage::Closed);
    }

    debug() << "Reserved locally: " << kReservationAmount;
    sendMessage<IntermediateNodeReservationResponseMessage>(
        kNeighbor,
        mEquivalent,
        mContractorsManager->ownAddresses(),
        currentTransactionUUID(),
        kReservation.pathID,
        ResponseMessage::Accepted,
        kReservationAmount);

    if (kTotalReserved == mTransactionAmount) {
        // Reserved amount is enough to move to votes processing stage.
        info() << "Requested amount collected.";

        // TODO: receiver must know, how many paths are involved into the transaction.
        // This info helps to calculate max timeout,
        // that would be used for waiting for votes list message.

        mStep = Stages::Coordinator_FinalAmountsConfigurationConfirmation;
        mTTLRequestWasSend = false;
        return resultWaitForMessageTypes( {
            Message::Payments_FinalAmountsConfiguration,
            Message::Payments_TransactionPublicKeyHash,
            Message::Payments_IntermediateNodeReservationRequest,
            Message::Payments_TTLProlongationResponse},
        maxNetworkDelay(kMaxPathLength - 1));
    }

    // Waiting for another reservation request
    return resultWaitForMessageTypes( {
        Message::Payments_IntermediateNodeReservationRequest,
        Message::Payments_TTLProlongationResponse},
    maxNetworkDelay((kMaxPathLength - 1) * 4));
}

TransactionResult::SharedConst ReceiverExchangePaymentTransaction::runFinalAmountsConfigurationConfirmation()
{
    debug() << "runFinalAmountsConfigurationConfirmation";
    if (contextIsValid(Message::Payments_IntermediateNodeReservationRequest, false)) {
        // todo : if final amounts configuration was received, ignore this message
        mStep = Receiver_AmountReservationsProcessing;
        return runAmountReservationStage();
    }

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
            debug() << "Transactions is still alive. Continue waiting for messages";
            if (utc_now() - mTimeStarted > kMaxTransactionDuration()) {
                return reject("Transaction duration time has expired. Rolling back");
            }
            return resultWaitForMessageTypes( {
                Message::Payments_IntermediateNodeReservationRequest,
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
        return reject("No all final configuration received. Transaction will be closed. Rolling Back");
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
        Message::Payments_TTLProlongationResponse,
        Message::Payments_IntermediateNodeReservationRequest},
    maxNetworkDelay(2));
}

TransactionResult::SharedConst ReceiverExchangePaymentTransaction::runFinalReservationsCoordinatorConfirmation()
{
    debug() << "runFinalReservationsCoordinatorConfirmation";
#ifdef TESTS
    mSubsystemsController->testForbidSendMessageOnFinalAmountClarificationStage();
#endif

    auto kMessage = popNextMessage<FinalAmountsConfigurationMessage>();
    auto coordinatorAddress = kMessage->senderAddresses.at(0);
    // todo : check coordinator

    mMaximalClaimingBlockNumber = kMessage->maximalClaimingBlockNumber();
    debug() << "maximal claiming block number on Coordinator side: " << mMaximalClaimingBlockNumber;
    mDisputeGracePeriodBlocksCount = kMessage->disputeGracePeriodBlocksCount();
    debug() << "dispute grace period blocks count on Coordinator side: " << mDisputeGracePeriodBlocksCount;

    debug() << "final amounts configuration size: " << kMessage->finalAmountsConfiguration().size();
    for (const auto &finalAmount : kMessage->finalAmountsConfiguration()) {
        debug() << "final amount: " << *finalAmount.amount 
                << " equivalent: " << finalAmount.equivalent
                << " pathID: " << finalAmount.pathID;
    }

    if (!updateReservations(
                kMessage->finalAmountsConfiguration())) {
        removeAllDataFromStorageConcerningTransaction();
        sendErrorMessageOnFinalAmountsConfiguration();
        // todo : discuss if receiver can reject TA on this stage or should wait
        return reject("There are some final amounts, reservations for which are absent. Rejected");
    }

#ifdef TESTS
    // coordinator wait for this message maxNetworkDelay(6)
    mSubsystemsController->testSleepOnFinalAmountClarificationStage(
        maxNetworkDelay(8));
#endif

    debug() << "All reservations were updated";
    if (!checkReservationsDirections()) {
        removeAllDataFromStorageConcerningTransaction();
        sendErrorMessageOnFinalAmountsConfiguration();
        return reject("Reservations on node are invalid");
    }
    info() << "All reservations directions are correct";

    mPaymentParticipants = kMessage->paymentParticipants();
    // todo check if current node is present in mPaymentParticipants
    for (const auto &paymentNodeIdAndAddress : mPaymentParticipants) {
        mPaymentNodesIds.insert(
            make_pair(
                paymentNodeIdAndAddress.second->mainAddress()->fullAddress(),
                paymentNodeIdAndAddress.first));
    }
    if (!checkAllNeighborsWithReservationsAreInFinalParticipantsList()) {
        removeAllDataFromStorageConcerningTransaction();
        sendErrorMessageOnFinalAmountsConfiguration();
        return reject("Node has reservation with participant, which not included in mPaymentNodesIds. Rejected");
    }

    auto ioTransaction = mStorageHandler->beginTransaction();
    auto *const trustLines = trustLinesManager(mEquivalent);

    if (kMessage->isReceiptContains()) {
        info() << "Coordinator also send receipt";
        auto coordinatorID = mContractorsManager->contractorIDByAddress(kMessage->senderAddresses.at(0));
        if (coordinatorID == ContractorsManager::kNotFoundContractorID) {
            removeAllDataFromStorageConcerningTransaction(ioTransaction);
            sendErrorMessageOnFinalAmountsConfiguration();
            return reject("Coordinator is not a neighbor. Rejected");
        }
        auto coordinatorTotalIncomingReservationAmount = totalReservedIncomingAmountToNode(
                coordinatorID,
                mEquivalent);
        auto keyChain = mKeysStore->keychain(
                            trustLines->trustLineID(coordinatorID));
        auto serializedIncomingReceiptData = getSerializedReceipt(
                coordinatorID,
                mContractorsManager->idOnContractorSide(coordinatorID),
                coordinatorTotalIncomingReservationAmount,
                false,
                mEquivalent);
        if (!keyChain.checkSign(
                    ioTransaction,
                    serializedIncomingReceiptData.first,
                    serializedIncomingReceiptData.second,
                    kMessage->signatures()[0].second)) {
            removeAllDataFromStorageConcerningTransaction(ioTransaction);
            sendErrorMessageOnFinalAmountsConfiguration();
            return reject("Coordinator send invalid receipt signature. Rejected");
        }
        if (!keyChain.saveIncomingPaymentReceipt(
                    ioTransaction,
                    trustLines->auditNumber(coordinatorID),
                    mTransactionUUID,
                    coordinatorTotalIncomingReservationAmount,
                    kMessage->signatures()[0].second)) {
            removeAllDataFromStorageConcerningTransaction(ioTransaction);
            sendErrorMessageOnFinalAmountsConfiguration();
            return reject("Can't save coordinator receipt. Rejected.");
        }
        info() << "Coordinator's receipt is valid";
    } else {
        auto coordinatorID = mContractorsManager->contractorIDByAddress(kMessage->senderAddresses.at(0));
        if (coordinatorID != ContractorsManager::kNotFoundContractorID) {
            auto coordinatorTotalIncomingReservationAmount = totalReservedIncomingAmountToNode(
                    coordinatorID,
                    mEquivalent);
            if (coordinatorTotalIncomingReservationAmount != TrustLine::kZeroAmount()) {
                removeAllDataFromStorageConcerningTransaction(ioTransaction);
                sendErrorMessageOnFinalAmountsConfiguration();
                warning() << "Receipt amount: 0. Local incoming amount: "
                          << coordinatorTotalIncomingReservationAmount;
                return reject("Coordinator send invalid receipt amount. Rejected");
            }
        }
    }

    mStep = Common_ObservingBlockNumberProcessing;
    sendRpcRequest(
        make_shared<GetBlockNumberRpcRequest>(
            mTransactionUUID));
    mBlockNumberObtainingInProcess = true;
    return resultWaitForRpcResponseAndMessagesTypes(
        RpcMethod::GetBlockNumber,
        {Message::Payments_TransactionPublicKeyHash,
        Message::Payments_TTLProlongationResponse},
        maxNetworkDelay(1));
}

TransactionResult::SharedConst ReceiverExchangePaymentTransaction::runFinalReservationsNeighborConfirmation()
{
    debug() << "runFinalReservationsNeighborConfirmation";
    auto kMessage = popNextMessage<TransactionPublicKeyHashMessage>();
    auto senderAddress = kMessage->senderAddresses.at(0);
    debug() << "sender: " << senderAddress->fullAddress();

    mParticipantsPublicKeysHashes[senderAddress->fullAddress()].first = kMessage->paymentNodeID();
    mParticipantsPublicKeysHashes[senderAddress->fullAddress()].second = kMessage->transactionPublicKeyHash();

    auto ioTransaction = mStorageHandler->beginTransaction();
    auto *const trustLines = trustLinesManager(mEquivalent);
    if (kMessage->isReceiptContains()) {
        info() << "Sender also send receipt";
        auto senderID = mContractorsManager->contractorIDByAddress(senderAddress);
        if (senderID == ContractorsManager::kNotFoundContractorID) {
            removeAllDataFromStorageConcerningTransaction(ioTransaction);
            sendErrorMessageOnFinalAmountsConfiguration();
            return reject("Sender is not a neighbor. Rejected");
        }
        auto participantTotalIncomingReservationAmount = totalReservedIncomingAmountToNode(
                senderID,
                mEquivalent);
        auto keyChain = mKeysStore->keychain(
                            trustLines->trustLineID(senderID));
        auto serializedIncomingReceiptData = getSerializedReceipt(
                senderID,
                mContractorsManager->idOnContractorSide(senderID),
                participantTotalIncomingReservationAmount,
                false,
                mEquivalent);
        if (!keyChain.checkSign(
                    ioTransaction,
                    serializedIncomingReceiptData.first,
                    serializedIncomingReceiptData.second,
                    kMessage->signatures()[0].second)) {
            removeAllDataFromStorageConcerningTransaction(ioTransaction);
            sendErrorMessageOnFinalAmountsConfiguration();
            return reject("Sender send invalid receipt signature. Rejected");
        }
        if (!keyChain.saveIncomingPaymentReceipt(
                    ioTransaction,
                    trustLines->auditNumber(senderID),
                    mTransactionUUID,
                    participantTotalIncomingReservationAmount,
                    kMessage->signatures()[0].second)) {
            removeAllDataFromStorageConcerningTransaction(ioTransaction);
            sendErrorMessageOnFinalAmountsConfiguration();
            return reject("Can't save participant receipt. Rejected.");
        }
        info() << "Sender's receipt is valid";
    } else {
        auto senderID = mContractorsManager->contractorIDByAddress(senderAddress);
        if (senderID != ContractorsManager::kNotFoundContractorID) {
            auto participantTotalIncomingReservationAmount = totalReservedIncomingAmountToNode(
                    senderID,
                    mEquivalent);
            if (participantTotalIncomingReservationAmount != TrustLine::kZeroAmount()) {
                removeAllDataFromStorageConcerningTransaction(ioTransaction);
                sendErrorMessageOnFinalAmountsConfiguration();
                warning() << "Receipt amount: 0. Local reserved incoming amount: "
                          << participantTotalIncomingReservationAmount;
                return reject("Sender send invalid receipt amount. Rejected");
            }
        }
    }

    // if coordinator didn't sent final payment configuration yet
    if (mPaymentParticipants.empty()) {
        return resultWaitForMessageTypes( {
            Message::Payments_FinalAmountsConfiguration,
            Message::Payments_TransactionPublicKeyHash,
            Message::Payments_TTLProlongationResponse},
        maxNetworkDelay(1));
    }

    if (mBlockNumberObtainingInProcess) {
        return resultWaitForRpcResponseAndMessagesTypes(
        RpcMethod::GetBlockNumber,
        {Message::Payments_TransactionPublicKeyHash},
        maxNetworkDelay(1));
    }

    // coordinator already sent final amounts configuration
    // coordinator don't send public key hash
    if (mPaymentParticipants.size() == mParticipantsPublicKeysHashes.size() + 1) {
        // all neighbors sent theirs reservations
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

    // not all neighbors sent theirs reservations
    return resultWaitForMessageTypes( {
        Message::Payments_TransactionPublicKeyHash,
        Message::Payments_TTLProlongationResponse},
    maxNetworkDelay(2));
}

TransactionResult::SharedConst ReceiverExchangePaymentTransaction::runCheckObservingBlockNumber()
{
    info() << "runCheckObservingBlockNumber";
    if (contextIsValid(Message::Payments_TransactionPublicKeyHash, false)) {
        return runFinalReservationsNeighborConfirmation();
    }

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
        removeAllDataFromStorageConcerningTransaction();
        sendErrorMessageOnFinalAmountsConfiguration();
        return reject("Max claiming block number sending by coordinator is invalid . Rejected.");
    }
    if (mDisputeGracePeriodBlocksCount != kDisputeGracePeriodBlocksCount) {
        removeAllDataFromStorageConcerningTransaction();
        sendErrorMessageOnFinalAmountsConfiguration();
        return reject("Dispute grace period blocks count is not properly. Rejected.");
    }
    mBlockNumberObtainingInProcess = false;

    auto ioTransaction = mStorageHandler->beginTransaction();
    mKeysStore->ensurePaymentKeyExists(ioTransaction);
    mPublicKey = ioTransaction->paymentKeysHandler()->getOwnPublicKey();
    mParticipantsPublicKeysHashes.insert(
        make_pair(
            mContractorsManager->selfContractor()->mainAddress()->fullAddress(),
            make_pair(
                mPaymentNodesIds[mContractorsManager->selfContractor()->mainAddress()->fullAddress()],
                mPublicKey->hash())));

    // send public key hash to all participants except coordinator
    auto ownPaymentID = mPaymentNodesIds[mContractorsManager->selfContractor()->mainAddress()->fullAddress()];
    for (const auto &paymentNodeIdAndContractor : mPaymentParticipants) {
        if (paymentNodeIdAndContractor.second == mCoordinator) {
            continue;
        }
        if (paymentNodeIdAndContractor.second == mContractorsManager->selfContractor()) {
            continue;
        }
        info() << "Send public key hash to " << paymentNodeIdAndContractor.second->mainAddress()->fullAddress();
        sendMessage<TransactionPublicKeyHashMessage>(
            paymentNodeIdAndContractor.second->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            ownPaymentID,
            mPublicKey->hash());
    }

    // coordinator don't send public key hash
    if (mPaymentParticipants.size() == mParticipantsPublicKeysHashes.size() + 1) {
        info() << "All neighbors send theirs reservations";
        // all neighbors sent theirs reservations
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
        maxNetworkDelay(5)); // todo : need discuss this parameter (5)
    }

    mStep = Coordinator_FinalAmountsConfigurationConfirmation;
    return resultWaitForMessageTypes( {
        Message::Payments_TransactionPublicKeyHash,
        Message::Payments_TTLProlongationResponse},
    maxNetworkDelay(2));
}


TransactionResult::SharedConst ReceiverExchangePaymentTransaction::runVotesStageWithCoordinatorClarification()
{
    debug() << "runVotesStageWithCoordinatorClarification";
    if (contextIsValid(Message::Payments_TTLProlongationResponse, false)) {
        debug() << "Receive TTL prolongation message";
        const auto kMessage = popNextMessage<TTLProlongationResponseMessage>();
        if (kMessage->state() == TTLProlongationResponseMessage::Continue) {
            debug() << "Transactions is still alive. Continue waiting for messages";
            if (utc_now() - mTimeStarted > kMaxTransactionDuration()) {
                return reject("Transaction duration time has expired. Rolling back");
            }
            return resultWaitForMessageTypes( {
                Message::Payments_TTLProlongationResponse,
                Message::Payments_ParticipantsPublicKeys},
            maxNetworkDelay((kMaxPathLength - 1) * 4));
        }
        removeAllDataFromStorageConcerningTransaction();
        return reject("Coordinator send TTL message with transaction finish state. Rolling Back");
    }

    if (contextIsValid(Message::Payments_ParticipantsPublicKeys, false)) {
        if (mTransactionShouldBeRejected) {
            // this case can happens only with Receiver,
            // when coordinator wants to reserve greater then command amount
            removeAllDataFromStorageConcerningTransaction();
            reject("Receiver rejected transaction because of discrepancy reservations with Coordinator. Rolling back.");
        }
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

TransactionResult::SharedConst ReceiverExchangePaymentTransaction::approve()
{
    mCommittedAmount = totalReservedAmount(
                           AmountReservation::Incoming,
                           mEquivalent);
    BaseExchangePaymentTransaction::approve();

    try {
        mEventsInterfaceManager->writeEvent(
            Event::paymentIncomingEvent(
                mCoordinator->mainAddress(),
                mContractorsManager->selfContractor()->mainAddress(),
                mCommittedAmount,
                mEquivalent,
                mPayload));
    } catch (std::exception &e) {
        warning() << "Can't write payment event " << e.what();
    }

    return resultDone();
}

void ReceiverExchangePaymentTransaction::savePaymentOperationIntoHistory(
    IOTransaction::Shared ioTransaction)
{
    debug() << "savePaymentOperationIntoHistory";
    if (mPaymentParticipants.count(kCoordinatorPaymentNodeID) == 0) {
        warning() << "Can't identify coordinator node";
        // todo : need correct reaction
    }
    auto coordinatorAddress = mPaymentParticipants[kCoordinatorPaymentNodeID];
    auto *const trustLines = trustLinesManager(mEquivalent);
    ioTransaction->historyStorage()->savePaymentRecord(
        make_shared<PaymentRecord>(
            mEquivalent,
            currentTransactionUUID(),
            PaymentRecord::PaymentOperationType::IncomingPaymentType,
            mPaymentParticipants[kCoordinatorPaymentNodeID],
            mCommittedAmount,
            *trustLines->totalBalance().get(),
            mOutgoingTransfers,
            mIncomingTransfers,
            mPayload));
    debug() << "Operation saved";
}

bool ReceiverExchangePaymentTransaction::checkReservationsDirections() const
{
    debug() << "checkReservationsDirections";

    TrustLineAmount totalIncoming = TrustLineAmount(0);
    bool hasIncoming = false;

    for (const auto& [contractorID, reservations] : mReservations) {
        for (const auto& [pathID, reservation] : reservations) {
            if (reservation->direction() == AmountReservation::Incoming) {
                // Validate all incoming reservations are in receiver's equivalent (mEquivalent)
                if (reservation->equivalent() != mEquivalent) {
                    warning() << "checkReservationsDirections: Incoming reservation in wrong equivalent. "
                              << "Expected: " << mEquivalent << ", got: " << reservation->equivalent();
                    return false;
                }
                totalIncoming = totalIncoming + reservation->amount();
                hasIncoming = true;
            }
        }
    }

    // Validate we have incoming reservations
    return hasIncoming && totalIncoming > TrustLineAmount(0);
}

TransactionResult::SharedConst ReceiverExchangePaymentTransaction::sendErrorMessageOnPreviousNodeRequest(
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

    // TODO: enhancement: send approximate paths count to receiver, so it will be able to wait correct timeout.
    return resultWaitForMessageTypes( {
        Message::Payments_IntermediateNodeReservationRequest,
        Message::Payments_TTLProlongationResponse},
    maxNetworkDelay((kMaxPathLength - 1) * 4));
}

void ReceiverExchangePaymentTransaction::sendErrorMessageOnFinalAmountsConfiguration()
{
    sendMessage<FinalAmountsConfigurationResponseMessage>(
        mCoordinator->mainAddress(),
        mEquivalent,
        mContractorsManager->ownAddresses(),
        mTransactionUUID,
        FinalAmountsConfigurationResponseMessage::Rejected);
}

BaseAddress::Shared ReceiverExchangePaymentTransaction::coordinatorAddress() const
{
    return mCoordinator->mainAddress();
}

const string ReceiverExchangePaymentTransaction::logHeader() const
{
    stringstream s;
    s << "[ReceiverExchangePaymentTA: " << currentTransactionUUID().stringUUID() << " " << mEquivalent << "] ";
    return s.str();
}
