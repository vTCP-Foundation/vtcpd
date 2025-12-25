#include "BaseExchangePaymentTransaction.h"

// Out-of-class definition for ODR-used static const member
const uint16_t BaseExchangePaymentTransaction::kDisputeGracePeriodBlocksCount;

BaseExchangePaymentTransaction::BaseExchangePaymentTransaction(
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
    SubsystemsController *subsystemsController) :

    BaseTransaction(
        type,
        equivalent,
        log),
    mContractorsManager(contractorsManager),
    mEquivalentsSubsystemsRouter(equivalentsSubsystemsRouter),
    mStorageHandler(storageHandler),
    mResourcesManager(resourcesManager),
    mExchangePathsManager(exchangePathsManager),
    mObservingHandler(observingHandler),
    mKeysStore(keystore),
    mSubsystemsController(subsystemsController),
    mTTLRequestWasSend(false),
    mTransactionIsVoted(false),
    mParticipantsVotesMessage(nullptr),
    mBlockNumberObtainingInProcess(false),
    mSignedTransaction(nullptr),
    mIsSuspendedOnFinalAmountsConfirmationStage(false),
    mCntSuspendingOnFinalAmountsConfirmationStage(0),
    mPayload("")
{
}

BaseExchangePaymentTransaction::BaseExchangePaymentTransaction(
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
    SubsystemsController *subsystemsController) :

    BaseTransaction(
        type,
        transactionUUID,
        equivalent,
        log),
    mContractorsManager(contractorsManager),
    mEquivalentsSubsystemsRouter(equivalentsSubsystemsRouter),
    mStorageHandler(storageHandler),
    mResourcesManager(resourcesManager),
    mExchangePathsManager(exchangePathsManager),
    mObservingHandler(observingHandler),
    mKeysStore(keystore),
    mSubsystemsController(subsystemsController),
    mTTLRequestWasSend(false),
    mTransactionIsVoted(false),
    mParticipantsVotesMessage(nullptr),
    mBlockNumberObtainingInProcess(false),
    mSignedTransaction(nullptr),
    mIsSuspendedOnFinalAmountsConfirmationStage(false),
    mCntSuspendingOnFinalAmountsConfirmationStage(0),
    mPayload("")
{
}

BaseExchangePaymentTransaction::BaseExchangePaymentTransaction(
    BytesShared buffer,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    StorageHandler *storageHandler,
    ResourcesManager *resourcesManager,
    ExchangePathsManager *exchangePathsManager,
    ObservingHandler *observingHandler,
    Keystore *keystore,
    Logger &log,
    SubsystemsController *subsystemsController) :

    BaseTransaction(
        buffer,
        log),
    mTransactionIsVoted(true),
    mContractorsManager(contractorsManager),
    mEquivalentsSubsystemsRouter(equivalentsSubsystemsRouter),
    mStorageHandler(storageHandler),
    mResourcesManager(resourcesManager),
    mExchangePathsManager(exchangePathsManager),
    mObservingHandler(observingHandler),
    mKeysStore(keystore),
    mSubsystemsController(subsystemsController),
    mTTLRequestWasSend(false),
    mParticipantsVotesMessage(nullptr),
    mBlockNumberObtainingInProcess(false),
    mSignedTransaction(nullptr),
    mIsSuspendedOnFinalAmountsConfirmationStage(false),
    mCntSuspendingOnFinalAmountsConfirmationStage(0),
    mPayload(""),
    mCountRecoveryAttempts(0)
{
    auto bytesBufferOffset = BaseTransaction::kOffsetToInheritedBytes();

    // mReservations count
    SerializedRecordsCount reservationsCount;
    memcpy(
        &reservationsCount,
        buffer.get() + bytesBufferOffset,
        sizeof(SerializedRecordsCount));
    bytesBufferOffset += sizeof(SerializedRecordsCount);

    // Map values
    for (auto idx = 0; idx < reservationsCount; idx++) {
        // Map Key ContractorID
        ContractorID stepContractorID;
        memcpy(
            &stepContractorID,
            buffer.get() + bytesBufferOffset,
            sizeof(ContractorID));
        bytesBufferOffset += sizeof(ContractorID);

        // Map values vector
        SerializedRecordsCount stepReservationVectorSize;
        memcpy(
            &stepReservationVectorSize,
            buffer.get() + bytesBufferOffset,
            sizeof(SerializedRecordsCount));
        bytesBufferOffset += sizeof(SerializedRecordsCount);

        for (auto jdx = 0; jdx < stepReservationVectorSize; jdx++) {

            // PathID
            PathID stepPathID;
            memcpy(
                &stepPathID,
                buffer.get() + bytesBufferOffset,
                sizeof(PathID));
            bytesBufferOffset += sizeof(PathID);

            // Amount
            TrustLineAmount stepAmount;
            vector<byte_t> amountBytes(
                buffer.get() + bytesBufferOffset,
                buffer.get() + bytesBufferOffset + kTrustLineAmountBytesCount);
            stepAmount = bytesToTrustLineAmount(amountBytes);
            bytesBufferOffset += kTrustLineAmountBytesCount;

            // Direction
            AmountReservation::SerializedReservationDirectionSize stepDirection;
            memcpy(
                &stepDirection,
                buffer.get() + bytesBufferOffset,
                sizeof(AmountReservation::SerializedReservationDirectionSize));
            bytesBufferOffset += sizeof(AmountReservation::SerializedReservationDirectionSize);
            auto stepEnumDirection = static_cast<AmountReservation::ReservationDirection>(stepDirection);

            // Equivalent
            SerializedEquivalent stepEquivalent;
            memcpy(
                &stepEquivalent,
                buffer.get() + bytesBufferOffset,
                sizeof(SerializedEquivalent));
            bytesBufferOffset += sizeof(SerializedEquivalent);

            // Get the appropriate TrustLinesManager for this equivalent
            auto trustLineManager = mEquivalentsSubsystemsRouter->trustLinesManager(stepEquivalent);

            bool isReserveAmounts = !trustLineManager->isReservationsPresentConsiderTransaction(
                                        mTransactionUUID);

            if (isReserveAmounts) {
                if (stepEnumDirection == AmountReservation::ReservationDirection::Incoming) {
                    if (!reserveIncomingAmount(
                                stepContractorID,
                                stepAmount,
                                stepPathID,
                                stepEquivalent)) {
                        // can't create reserve, but this reserve was serialized before node dropping
                        // we must stop this node and find out the reason
                        exit(1);
                    }
                }

                if (stepEnumDirection == AmountReservation::ReservationDirection::Outgoing) {
                    if (!reserveOutgoingAmount(
                                stepContractorID,
                                stepAmount,
                                stepPathID,
                                stepEquivalent)) {
                        // can't create reserve, but this reserve was serialized before node dropping
                        // we must stop this node and find out the reason
                        exit(1);
                    }
                }
            } else {
                if (!copyReservationFromGlobalReservations(
                            stepContractorID,
                            stepAmount,
                            stepEnumDirection,
                            stepPathID,
                            stepEquivalent)) {
                    // can't get reserve from AmountReservationsHandler, but this reserve must be
                    // we must stop this node and find out the reason
                    exit(1);
                }
            }
        }
    }

    // Participants paymentIDs and public keys Part
    SerializedRecordsCount kTotalParticipantsCount;
    memcpy(
        &kTotalParticipantsCount,
        buffer.get() + bytesBufferOffset,
        sizeof(SerializedRecordsCount));
    bytesBufferOffset += sizeof(SerializedRecordsCount);

    for (SerializedRecordNumber idx = 0; idx < kTotalParticipantsCount; idx++) {
        // Read PaymentNodeID in an alignment-safe way
        PaymentNodeID paymentNodeID;
        memcpy(
            &paymentNodeID,
            buffer.get() + bytesBufferOffset,
            sizeof(PaymentNodeID));
        bytesBufferOffset += sizeof(PaymentNodeID);
        //---------------------------------------------------
        auto participantContractor = make_shared<Contractor>(buffer.get() + bytesBufferOffset);
        bytesBufferOffset += participantContractor->serializedSize();

        mPaymentParticipants.insert(
            make_pair(
                paymentNodeID,
                participantContractor));

        auto publicKey = make_shared<sphincs::PublicKey>(
                             buffer.get() + bytesBufferOffset);
        bytesBufferOffset += sphincs::PublicKey::keySize();

        mParticipantsPublicKeys.insert(
            make_pair(
                paymentNodeID,
                publicKey));
    }

    memcpy(
        &mMaximalClaimingBlockNumber,
        buffer.get() + bytesBufferOffset,
        sizeof(BlockNumber));
    bytesBufferOffset += sizeof(BlockNumber);

    // Read payload length safely and always advance offset
    byte_t payloadLength = 0;
    memcpy(
        &payloadLength,
        buffer.get() + bytesBufferOffset,
        sizeof(byte_t));
    bytesBufferOffset += sizeof(byte_t);

    if (payloadLength > 0) {
        mPayload = string(
                       buffer.get() + bytesBufferOffset,
                       buffer.get() + bytesBufferOffset + payloadLength);
        // bytesBufferOffset += payloadLength; // not used further, so not strictly required
    } else {
        mPayload.clear();
    }

    if (mStep != Stages::Common_Observing) {
        mStep = Stages::Common_Recovery;
        mVotesRecoveryStep = Common_PrepareNodesListToCheckVotes;
    }
}

// Helper methods for accessing equivalent-specific managers
TrustLinesManager* BaseExchangePaymentTransaction::trustLinesManager(
    const SerializedEquivalent equivalent) const
{
    return mEquivalentsSubsystemsRouter->trustLinesManager(equivalent);
}

TopologyCacheManager* BaseExchangePaymentTransaction::topologyCacheManager(
    const SerializedEquivalent equivalent) const
{
    return mEquivalentsSubsystemsRouter->topologyCacheManager(equivalent);
}

MaxFlowCacheManager* BaseExchangePaymentTransaction::maxFlowCacheManager(
    const SerializedEquivalent equivalent) const
{
    return mEquivalentsSubsystemsRouter->maxFlowCacheManager(equivalent);
}

bool BaseExchangePaymentTransaction::iAmGateway(
    const SerializedEquivalent equivalent) const
{
    return mEquivalentsSubsystemsRouter->iAmGateway(equivalent);
}

// Serialization implementation for multi-equivalent payment transactions
pair<BytesShared, size_t> BaseExchangePaymentTransaction::serializeToBytes() const
{
    const auto parentBytesAndCount = BaseTransaction::serializeToBytes();
    size_t bytesCount = parentBytesAndCount.second + sizeof(SerializedRecordsCount) + reservationsSizeInBytes() + sizeof(SerializedRecordsCount) + sizeof(BlockNumber) + sizeof(byte_t) + mPayload.length();
    for (const auto &participant : mPaymentParticipants) {
        bytesCount += sizeof(PaymentNodeID) + participant.second->serializedSize() + sphincs::PublicKey::keySize();
    }

    BytesShared dataBytesShared = tryCalloc(bytesCount);
    size_t dataBytesOffset = 0;

    // Parent part
    memcpy(
        dataBytesShared.get(),
        parentBytesAndCount.first.get(),
        parentBytesAndCount.second);
    dataBytesOffset += parentBytesAndCount.second;

    auto kmReservationSize = (SerializedRecordsCount)mReservations.size();
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &kmReservationSize,
        sizeof(SerializedRecordsCount));
    dataBytesOffset += sizeof(SerializedRecordsCount);

    for (const auto &nodeAndReservations : mReservations) {
        // Map key (ContractorID)
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            &nodeAndReservations.first,
            sizeof(ContractorID));
        dataBytesOffset += sizeof(ContractorID);

        // Size of map value vector
        auto kReservationsValueSize = (SerializedRecordsCount)nodeAndReservations.second.size();
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            &kReservationsValueSize,
            sizeof(SerializedRecordsCount));
        dataBytesOffset += sizeof(SerializedRecordsCount);

        for (const auto &kReservationValues : nodeAndReservations.second) {
            // PathID
            memcpy(
                dataBytesShared.get() + dataBytesOffset,
                &kReservationValues.first,
                sizeof(PathID));
            dataBytesOffset += sizeof(PathID);

            // AmountReservation - TrustLineAmount
            vector<byte_t> buffer = trustLineAmountToBytes(
                                        kReservationValues.second->amount());
            memcpy(
                dataBytesShared.get() + dataBytesOffset,
                buffer.data(),
                buffer.size());
            dataBytesOffset += buffer.size();

            // Direction
            const auto kDirection = kReservationValues.second->direction();
            memcpy(
                dataBytesShared.get() + dataBytesOffset,
                &kDirection,
                sizeof(AmountReservation::SerializedReservationDirectionSize));
            dataBytesOffset += sizeof(AmountReservation::SerializedReservationDirectionSize);

            // Equivalent
            const auto kEquivalent = kReservationValues.second->equivalent();
            memcpy(
                dataBytesShared.get() + dataBytesOffset,
                &kEquivalent,
                sizeof(SerializedEquivalent));
            dataBytesOffset += sizeof(SerializedEquivalent);
        }
    }

    // Participants paymentIDs and public keys Part
    auto kTotalParticipantsCount = mPaymentParticipants.size();
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &kTotalParticipantsCount,
        sizeof(SerializedRecordsCount));
    dataBytesOffset += sizeof(SerializedRecordsCount);

    // NodePaymentIDs and payment contractor
    for (auto const &paymentNodeIdAndContractor : mPaymentParticipants) {
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            &paymentNodeIdAndContractor.first,
            sizeof(PaymentNodeID));
        dataBytesOffset += sizeof(PaymentNodeID);

        auto contractorSerializedData = paymentNodeIdAndContractor.second->serializeToBytes();
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            contractorSerializedData.get(),
            paymentNodeIdAndContractor.second->serializedSize());
        dataBytesOffset += paymentNodeIdAndContractor.second->serializedSize();

        auto participantPublicKey = mParticipantsPublicKeys.at(paymentNodeIdAndContractor.first);
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            participantPublicKey->data(),
            sphincs::PublicKey::keySize());
        dataBytesOffset += sphincs::PublicKey::keySize();
    }

    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &mMaximalClaimingBlockNumber,
        sizeof(BlockNumber));
    dataBytesOffset += sizeof(BlockNumber);

    auto payloadLength = (byte_t)mPayload.length();
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &payloadLength,
        sizeof(byte_t));

    if (payloadLength > 0) {
        dataBytesOffset += sizeof(byte_t);
        memcpy(
            dataBytesShared.get() + dataBytesOffset,
            mPayload.c_str(),
            payloadLength);
    }

    return make_pair(
               dataBytesShared,
               bytesCount);
}

BaseAddress::Shared BaseExchangePaymentTransaction::coordinatorAddress() const
{
    throw RuntimeError("BaseExchangePaymentTransaction::coordinatorAddress not yet implemented");
}

const SerializedPathLengthSize BaseExchangePaymentTransaction::cycleLength() const
{
    return 0;
}

bool BaseExchangePaymentTransaction::isVotingStage() const
{
    return mTransactionIsVoted;
}

void BaseExchangePaymentTransaction::setTransactionState(
    BaseExchangePaymentTransaction::SerializedStep transactionStage)
{
    // Implementation to be added in subsequent tasks
}

void BaseExchangePaymentTransaction::setObservingParticipantsSignatures(
    map<PaymentNodeID, sphincs::Signature::Shared> participantsSignatures)
{
    mParticipantsSignatures = participantsSignatures;
}

const bool BaseExchangePaymentTransaction::reserveOutgoingAmount(
    ContractorID neighborNode,
    const TrustLineAmount& amount,
    const PathID &pathID,
    const SerializedEquivalent equivalent)
{
    try {
        auto trustLineManager = trustLinesManager(equivalent);
        const auto kReservation = trustLineManager->reserveOutgoingAmount(
                                      neighborNode,
                                      currentTransactionUUID(),
                                      amount);

#ifdef DEBUG
        // todo: uncomment me, when problem with recoverin transaction and reservation after restarting node will be fixed
        // debug() << "Reserved " << amount << " for (" << neighborNode << ") [" << pathID << "] [Outgoing amount reservation] [equiv: " << equivalent << "].";
#endif

        mReservations[neighborNode].emplace_back(
            pathID,
            kReservation);
        return true;
    } catch (Exception &) {
    }

    return false;
}

const bool BaseExchangePaymentTransaction::reserveIncomingAmount(
    ContractorID neighborNode,
    const TrustLineAmount& amount,
    const PathID &pathID,
    const SerializedEquivalent equivalent)
{
    try {
        auto trustLineManager = trustLinesManager(equivalent);
        const auto kReservation = trustLineManager->reserveIncomingAmount(
                                      neighborNode,
                                      currentTransactionUUID(),
                                      amount);

#ifdef DEBUG
        // todo: uncomment me, when problem with recoverin transaction and reservation after restarting node will be fixed
        // debug() << "Reserved " << amount << " for (" << neighborNode << ") [" << pathID << "] [Incoming amount reservation] [equiv: " << equivalent << "].";
#endif

        mReservations[neighborNode].emplace_back(
            pathID,
            kReservation);
        return true;
    } catch (Exception &) {
    }

    return false;
}

const bool BaseExchangePaymentTransaction::copyReservationFromGlobalReservations(
    ContractorID neighborNode,
    const TrustLineAmount& amount,
    AmountReservation::ReservationDirection reservationDirection,
    const PathID &pathID,
    const SerializedEquivalent equivalent)
{
    try {
        auto trustLineManager = trustLinesManager(equivalent);
        auto reservation = trustLineManager->getAmountReservation(
                               neighborNode,
                               mTransactionUUID,
                               amount,
                               reservationDirection);
        mReservations[neighborNode].emplace_back(
            pathID,
            reservation);
        return true;
    } catch (NotFoundError &e) {
        warning() << "copyReservationFromGlobalReservations " << e.what();
        return false;
    }
}

const bool BaseExchangePaymentTransaction::shortageReservation(
    ContractorID kContractor,
    const AmountReservation::ConstShared kReservation,
    const TrustLineAmount &kNewAmount,
    const PathID &pathID,
    const SerializedEquivalent equivalent)
{
    debug() << "shortageReservation on path " << pathID << " for equivalent " << equivalent;
    if (kNewAmount > kReservation->amount()) {
        throw ValueError(
            "BaseExchangePaymentTransaction::shortageReservation: "
            "new amount can't be greater than already reserved one.");
    }

    // Validate equivalent matches
    if (kReservation->equivalent() != equivalent) {
        warning() << "shortageReservation: Equivalent mismatch. Expected: "
                  << kReservation->equivalent() << ", got: " << equivalent;
        return false;
    }

    try {
        // this field used only for debug output
        const auto kPreviousAmount = kReservation->amount();

        auto trustLineManager = mEquivalentsSubsystemsRouter->trustLinesManager(equivalent);
        auto updatedReservation = trustLineManager->updateAmountReservation(
                                      kContractor,
                                      kReservation,
                                      kNewAmount);

        for (auto it = mReservations[kContractor].begin(); it != mReservations[kContractor].end(); it++) {
            if ((*it).second.get() == kReservation.get() && (*it).first == pathID) {
                mReservations[kContractor].erase(it);
                break;
            }
        }
        mReservations[kContractor].emplace_back(
            pathID,
            updatedReservation);

        if (kReservation->direction() == AmountReservation::Incoming)
            debug() << "Reservation for (" << kContractor << ") [" << pathID << "] shortened "
                    << "from " << kPreviousAmount << " to " << kNewAmount << " [<=]";
        else
            debug() << "Reservation for (" << kContractor << ") [" << pathID << "] shortened "
                    << "from " << kPreviousAmount << " to " << kNewAmount << " [=>]";

        return true;
    } catch (NotFoundError &) {
        warning() << "shortageReservation: Reservation not found for update";
    }

    return false;
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::runVotesCheckingStage()
{
    debug() << "runVotesCheckingStage";
    // todo add new stage and remove mTransactionIsVoted
    if (mTransactionIsVoted) {
        return runVotesConsistencyCheckingStage();
    }

    if (!contextIsValid(Message::Payments_ParticipantsPublicKeys)) {
        removeAllDataFromStorageConcerningTransaction();
        return reject("No participants public keys received. Canceling.");
    }

    auto participantsPublicKeyMessage = popNextMessage<ParticipantsPublicKeysMessage>();
    auto coordinator = make_shared<Contractor>(participantsPublicKeyMessage->senderAddresses);
    debug() << "Votes message received from " << coordinator->mainAddress()->fullAddress();
    if (coordinator != mPaymentParticipants[kCoordinatorPaymentNodeID]) {
        warning() << "Wrong coordinator. Continue previous state";
        return resultContinuePreviousState();
    }

    mParticipantsPublicKeys = participantsPublicKeyMessage->publicKeys();

    // todo : check if received own public key is the same as local

    if (!checkPublicKeysAppropriate()) {
        removeAllDataFromStorageConcerningTransaction();
        sendMessage<ParticipantVoteMessage>(
            coordinator->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID());
        return reject("Public keys are not appropriate. Reject.");
    }

    try {
        debug() << "Serializing transaction";
        auto ioTransaction = mStorageHandler->beginTransaction();
        auto bytesAndCount = serializeToBytes();
        debug() << "Transaction serialized";
        ioTransaction->transactionHandler()->saveRecord(
            currentTransactionUUID(),
            bytesAndCount.first,
            bytesAndCount.second);
        debug() << "Transaction saved";

        auto serializedOwnVotesData = getSerializedParticipantsVotesData(
                                          make_shared<Contractor>(
                                              mContractorsManager->ownAddresses()));
        debug() << "Data prepared for signing";

        auto signature = mKeysStore->signPaymentTransaction(
                             ioTransaction,
                             serializedOwnVotesData.first,
                             serializedOwnVotesData.second);
        if (!signature.has_value()) {
            removeAllDataFromStorageConcerningTransaction();
            return reject("Can't sign the transaction. See logs for the details.");
        }

        mSignedTransaction = signature.value();
        debug() << "Voted +";
        mTransactionIsVoted = true;

        // Save payment transaction with effective claiming block number
        // that includes the dispute grace period for extended monitoring
        ioTransaction->paymentTransactionsHandler()->saveRecord(
            mTransactionUUID,
            mMaximalClaimingBlockNumber,
            mMaximalClaimingBlockNumber + mDisputeGracePeriodBlocksCount);
    } catch (IOError &e) {
        error() << "Can't sign the transaction. See logs for the details.";
        removeAllDataFromStorageConcerningTransaction();
        sendMessage<ParticipantVoteMessage>(
            coordinator->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID());
        return reject("Canceling.");
    }

#ifdef TESTS
    mSubsystemsController->testThrowExceptionOnVoteStage();
    mSubsystemsController->testTerminateProcessOnVoteStage();
    mSubsystemsController->testForbidSendMessageOnVoteConsistencyStage();
    // coordinator wait for this message maxNetworkDelay(6)
    mSubsystemsController->testSleepOnVoteConsistencyStage(
        maxNetworkDelay(8));
#endif

    debug() << "Signed transaction transferred to coordinator";
    sendMessage<ParticipantVoteMessage>(
        coordinator->mainAddress(),
        mEquivalent,
        mContractorsManager->ownAddresses(),
        currentTransactionUUID(),
        mSignedTransaction);

    mStep = Stages::Common_VotesChecking;
    return resultWaitForMessageTypes(
               {Message::Payments_ParticipantsVotes},
               maxNetworkDelay(6));
}

/*
 * Handles votes list message receiving or it's absence in case,
 * if current node already voted for the operation.
 * In this case transaction can't be simply cancelled,
 * it must be recovered through recover mechanism to keep data integrity.
 */
TransactionResult::SharedConst BaseExchangePaymentTransaction::runVotesConsistencyCheckingStage()
{
    debug() << "runVotesConsistencyCheckingStage";

    if (contextIsValid(Message::Payments_ParticipantsPublicKeys, false)) {
        warning() << "Receive ParticipantsPublicKeys again";
        auto participantsPublicKeyMessage = popNextMessage<ParticipantsPublicKeysMessage>();
        auto coordinator = make_shared<Contractor>(participantsPublicKeyMessage->senderAddresses);
        debug() << "Votes message received from " << coordinator->mainAddress()->fullAddress();
        if (coordinator != mPaymentParticipants[kCoordinatorPaymentNodeID]) {
            warning() << "Wrong coordinator. Continue previous state";
            return resultContinuePreviousState();
        }

        if (mSignedTransaction == nullptr) {
            warning() << "There are no signed transaction for sending to coordinator";
            return resultContinuePreviousState();
        }

        debug() << "Signed transaction transferred to coordinator";
        sendMessage<ParticipantVoteMessage>(
            coordinator->mainAddress(),
            mEquivalent,
            mContractorsManager->ownAddresses(),
            currentTransactionUUID(),
            mSignedTransaction);

        return resultWaitForMessageTypes( {
                   Message::Payments_ParticipantsVotes,
                   Message::Payments_ParticipantsPublicKeys},
               maxNetworkDelay(9));
    }

    if (! contextIsValid(Message::Payments_ParticipantsVotes)) {
        // In case if no votes are present - transaction can't be simply cancelled.
        // It must go through recovery stage to avoid inconsistency.
        return recover("No participants votes received.");
    }

#ifdef TESTS
    mSubsystemsController->testThrowExceptionOnVoteConsistencyStage();
    mSubsystemsController->testTerminateProcessOnVoteConsistencyStage();
#endif

    mParticipantsVotesMessage = popNextMessage<ParticipantsVotesMessage>();
    // todo : check if message from coordinator
    debug() << "Participants votes message received.";
    mParticipantsSignatures = mParticipantsVotesMessage->participantsSignatures();

    return processParticipantsVotesMessage();
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::processParticipantsVotesMessage()
{
    debug() << "processParticipantsVotesMessage";
    if (!checkSignaturesAppropriate()) {
        return recover("Participants signatures map is incorrect. Rolling back.");
    }
    info() << "All signatures are appropriate";
    auto coordinatorSignature = mParticipantsSignatures[kCoordinatorPaymentNodeID];
    auto coordinatorPublicKey = mParticipantsPublicKeys[kCoordinatorPaymentNodeID];
    auto coordinatorSerializedVotesData = getSerializedParticipantsVotesData(
                                              mPaymentParticipants[kCoordinatorPaymentNodeID]);
    if (!coordinatorSignature->verify(
                *coordinatorPublicKey,
                coordinatorSerializedVotesData.first.get(),
                coordinatorSerializedVotesData.second)) {

        recover("Final coordinator signature is wrong");
    }
    for (const auto &paymentNodeIdAndContractor : mPaymentParticipants) {
        if (paymentNodeIdAndContractor.second == mContractorsManager->selfContractor()) {
            // todo discuss if need check own sign
            continue;
        }
        if (paymentNodeIdAndContractor.first == kCoordinatorPaymentNodeID) {
            // coordinator already checked
            continue;
        }
        auto participantPublicKey = mParticipantsPublicKeys[paymentNodeIdAndContractor.first];
        auto participantSignature = mParticipantsSignatures[paymentNodeIdAndContractor.first];
        auto participantSerializedVotesData = getSerializedParticipantsVotesData(
                                                  paymentNodeIdAndContractor.second);
        if (!participantSignature->verify(
                    *participantPublicKey,
                    participantSerializedVotesData.first.get(),
                    participantSerializedVotesData.second)) {
            warning() << "Node " << paymentNodeIdAndContractor.second->mainAddress()->fullAddress() << " signature is wrong";
            // todo : can be recursive
            return recover("Consensus not achieved.");
        }
    }

    debug() << "Votes list correct. Consensus achieved.";
    return approve();
}

/*
 * Shortcut method for approving transaction.
 * Base class realisation contains logic for approving (and committing)
 * transaction on the nodes where it was launched by the message (and not by the command).
 *
 * Writes success record to the transactions log.
 *
 * WARN:
 * This method must be overloaded on the coordinator side.
 */
TransactionResult::SharedConst BaseExchangePaymentTransaction::approve()
{
    debug() << "Transaction approved. Committing.";
    auto ioTransaction = mStorageHandler->beginTransaction();
    commit(ioTransaction);
    ioTransaction->paymentTransactionsHandler()->updateTransactionState(
        mTransactionUUID,
        PaymentObservingState::Committed);
    savePaymentOperationIntoHistory(ioTransaction);
    return resultDone();
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::recover(const char* message)
{
    debug() << "recover";
    if (message != nullptr) {
        warning() << message;
    }

    // todo: expand this condition including observing stage
    if (mTransactionIsVoted) {
        mStep = Stages::Common_Recovery;
        mVotesRecoveryStep = VotesRecoveryStages::Common_PrepareNodesListToCheckVotes;
        clearContext();
        mCountRecoveryAttempts = 0;
        return runVotesRecoveryParentStage();
    } else {
        warning() << "Transaction doesn't sent/receive participants votes message and will be closed";
        rollBack();
        return resultDone();
    }
}

/* Shortcut method for rejecting transaction.
 * Base class realisation contains logic for rejecting (and rolling back)
 * transaction on the nodes where it was launched by the message (and not by the command).
 *
 * Writes error record to the transactions log.
 *
 * WARN:
 * This method must be overloaded on the coordinator side.
 */
TransactionResult::SharedConst BaseExchangePaymentTransaction::reject(const char* message)
{
    if (message) {
        warning() << message;
    }

    rollBack();
    debug() << "Transaction successfully rolled back.";

    return resultDone();
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::runVotesRecoveryParentStage()
{
    debug() << "runVotesRecoveryParentStage";
    switch (mVotesRecoveryStep) {
    case VotesRecoveryStages ::Common_PrepareNodesListToCheckVotes:
        return runPrepareListNodesToCheckNodes();
    case VotesRecoveryStages ::Common_CheckCoordinatorVotesStage:
        return runCheckCoordinatorVotesStage();
    case VotesRecoveryStages ::Common_CheckIntermediateNodeVotesStage:
        return runCheckIntermediateNodeVotesStage();

    default:
        throw RuntimeError(
            "runVotesRecoveryParentStage::run(): "
            "invalid transaction step.");
    }
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::sendVotesRequestMessageAndWaitForResponse(
    Contractor::Shared contractor)
{
    debug() << "sendVotesRequestMessageAndWaitForResponse";
    sendMessage<VotesStatusRequestMessage>(
        contractor->mainAddress(),
        mEquivalent,
        mContractorsManager->ownAddresses(),
        currentTransactionUUID());

    debug() << "Send VotesStatusRequestMessage to " << contractor->mainAddress()->fullAddress();
    return resultWaitForMessageTypes(
               {Message::Payments_ParticipantsVotes},
               maxNetworkDelay(2));
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::processNextNodeToCheckVotes()
{
    debug() << "processNextNodeToCheckVotes";
    if (mNodesToCheckVotes.empty()) {
        debug() << "No nodes left to be asked";
        mCountRecoveryAttempts++;
        if (mCountRecoveryAttempts >= kMaxRecoveryAttempts) {
            debug() << "Max count recovery attempts. Send request to observers.";
            mObservingHandler->addPaymentClaim(
                mTransactionUUID,
                mMaximalClaimingBlockNumber,
                mParticipantsPublicKeys,
                mPublicKey,
                mSignedTransaction);
            return resultDone();
        }
        debug() << "Sleep and try again later";
        mVotesRecoveryStep = VotesRecoveryStages::Common_PrepareNodesListToCheckVotes;
        return resultAwakeAfterMilliseconds(
                   kWaitMillisecondsToTryRecoverAgain);
    }
    debug() << "Ask another node from payment transaction";
    mVotesRecoveryStep = VotesRecoveryStages::Common_CheckIntermediateNodeVotesStage;
    mCurrentNodeToCheckVotes = mNodesToCheckVotes.back();
    mNodesToCheckVotes.pop_back();
    return sendVotesRequestMessageAndWaitForResponse(
               mCurrentNodeToCheckVotes);
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::runPrepareListNodesToCheckNodes()
{
    debug() << "runPrepareListNodesToCheckNodes";
    // Add all nodes that could be asked for Votes Status.
    // Ignore self and Coordinator Node. Coordinator will be asked first
    Contractor::Shared coordinator = nullptr;
    for (const auto &paymentNodeIdAndContractor : mPaymentParticipants) {
        if (paymentNodeIdAndContractor.second == mContractorsManager->selfContractor()) {
            continue;
        }
        if (paymentNodeIdAndContractor.first == kCoordinatorPaymentNodeID) {
            coordinator = paymentNodeIdAndContractor.second;
            continue;
        }
        mNodesToCheckVotes.push_back(
            paymentNodeIdAndContractor.second);
    }
    mVotesRecoveryStep = VotesRecoveryStages::Common_CheckCoordinatorVotesStage;
    if (coordinator == nullptr) {
        warning() << "Participants list does not contain coordinator";
        return processNextNodeToCheckVotes();
    }
    return sendVotesRequestMessageAndWaitForResponse(coordinator);
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::runCheckCoordinatorVotesStage()
{
    debug() << "runCheckCoordinatorVotesStage";
    if (!contextIsValid(Message::Payments_ParticipantsVotes, false)) {
        if (mContext.empty()) {
            debug() << "Coordinator didn't send response";
            return processNextNodeToCheckVotes();
        }
        warning() << "receive message with invalid type, ignore it";
        clearContext();
        return resultContinuePreviousState();
    }

    const auto kMessage = popNextMessage<ParticipantsVotesMessage>();
    auto coordinator = make_shared<Contractor>(kMessage->senderAddresses);
    if (coordinator != mPaymentParticipants[kCoordinatorPaymentNodeID]) {
        warning() << "Sender " << coordinator->mainAddress()->fullAddress()
                  << " is not coordinator. Ignore this message";
        return resultContinuePreviousState();
    }
    if (kMessage->participantsSignatures().empty()) {
        debug() << "Coordinator don't know result of this transaction yet.";
        // todo : this is only for centralized model. TODO: Remove this condition.
        auto ioTransaction = mStorageHandler->beginTransaction();
        debug() << "rollback";
        removeAllDataFromStorageConcerningTransaction(ioTransaction);
        rollBack();
        return resultDone();
        //return processNextNodeToCheckVotes();
    }

    mParticipantsVotesMessage = kMessage;
    mParticipantsSignatures = mParticipantsVotesMessage->participantsSignatures();

    return processParticipantsVotesMessage();
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::runCheckIntermediateNodeVotesStage()
{
    debug() << "runCheckIntermediateNodeVotesStage";
    if (!contextIsValid(Message::Payments_ParticipantsVotes, false)) {
        if (mContext.empty()) {
            debug() << "Intermediate node didn't sent response";
            return processNextNodeToCheckVotes();
        }
        warning() << "receive message with invalid type, ignore it";
        clearContext();
        return resultContinuePreviousState();
    }

    const auto kMessage = popNextMessage<ParticipantsVotesMessage>();
    auto sender = make_shared<Contractor>(kMessage->senderAddresses);
    info() << "Node " << sender->mainAddress()->fullAddress() << " sent response";

    if (sender != mCurrentNodeToCheckVotes) {
        warning() << "Sender is not current checking node";
        return resultContinuePreviousState();
    }

    if (kMessage->participantsSignatures().empty()) {
        debug() << "Intermediate node didn't know about this transaction";
        return processNextNodeToCheckVotes();
    }

    mParticipantsVotesMessage = kMessage;
    mParticipantsSignatures = mParticipantsVotesMessage->participantsSignatures();

    return processParticipantsVotesMessage();
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::runObservingResultStage()
{
    info() << "runObservingResultStage";
    if (mParticipantsSignatures.empty()) {
        info() << "Participants signatures are empty. Close and wait for claiming result";
        return recover("Participants signatures are empty.");
    }
    info() << "Participants signatures receive";
    return processParticipantsVotesMessage();
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::runObservingRejectTransaction()
{
    info() << "runObservingRejectTransaction";
    auto ioTransaction = mStorageHandler->beginTransaction();
    removeAllDataFromStorageConcerningTransaction(ioTransaction);
    // todo : change transaction history status
    info() << "Reject by expiring claiming time";
    // todo : save observer reject signature
    rollBack();
    return resultDone();
}

void BaseExchangePaymentTransaction::saveVotes(IOTransaction::Shared ioTransaction)
{
    debug() << "saveVotes";
    for (const auto &paymentNodeIdAndContractor : mPaymentParticipants) {
        ioTransaction->paymentParticipantsVotesHandler()->saveRecord(
            mTransactionUUID,
            paymentNodeIdAndContractor.second,
            paymentNodeIdAndContractor.first,
            mParticipantsPublicKeys[paymentNodeIdAndContractor.first],
            mParticipantsSignatures[paymentNodeIdAndContractor.first]);
    }
}

void BaseExchangePaymentTransaction::commit(IOTransaction::Shared ioTransaction)
{
    debug() << "Transaction committing...";

    for (const auto &kNodeIDAndReservations : mReservations) {
        AmountReservation::ReservationDirection reservationDirection;
        for (const auto &kPathIDAndReservation : kNodeIDAndReservations.second) {
            // Get the appropriate TrustLinesManager for this reservation's equivalent
            auto trustLineManager = trustLinesManager(kPathIDAndReservation.second->equivalent());
            trustLineManager->useReservation(kNodeIDAndReservations.first, kPathIDAndReservation.second);

            if (kPathIDAndReservation.second->direction() == AmountReservation::Outgoing) {
                debug() << "Committed reservation: [ => ] " << kPathIDAndReservation.second->amount()
                        << " for (" << kNodeIDAndReservations.first << ", "
                        << mContractorsManager->contractorMainAddress(kNodeIDAndReservations.first)->fullAddress()
                        << ") [" << kPathIDAndReservation.first << "] [equiv: " << kPathIDAndReservation.second->equivalent() << "]";
                mContractorsForCycles.insert(
                    kNodeIDAndReservations.first);
                mOutgoingTransfers.emplace_back(
                    kNodeIDAndReservations.first,
                    kPathIDAndReservation.second->amount());
            } else if (kPathIDAndReservation.second->direction() == AmountReservation::Incoming) {
                debug() << "Committed reservation: [ <= ] " << kPathIDAndReservation.second->amount()
                        << " for (" << kNodeIDAndReservations.first << ", "
                        << mContractorsManager->contractorMainAddress(kNodeIDAndReservations.first)->fullAddress()
                        << ") [" << kPathIDAndReservation.first << "] [equiv: " << kPathIDAndReservation.second->equivalent() << "]";
                // For exchange transactions, check if iAmGateway for the specific equivalent
                if (iAmGateway(kPathIDAndReservation.second->equivalent())) {
                    // gateway try build cycles on both directions, because it don't shared by own routing tables
                    mContractorsForCycles.insert(
                        kNodeIDAndReservations.first);
                }
                mIncomingTransfers.emplace_back(
                    kNodeIDAndReservations.first,
                    kPathIDAndReservation.second->amount());
            }

            reservationDirection = kPathIDAndReservation.second->direction();
            trustLineManager->dropAmountReservation(
                kNodeIDAndReservations.first,
                kPathIDAndReservation.second);
        }
        // TODO : need to send signal in all equivalents involved
        trustLineActionSignal(
            kNodeIDAndReservations.first,
            mEquivalent,
            reservationDirection == AmountReservation::Outgoing);
    }

    // delete transaction references on dropped reservations
    mReservations.clear();

    // reset initiator cache, because after changing balances
    // we need updated information on max flow calculation transaction
    // For exchange transactions, we need to reset cache for all equivalents involved
    for (const auto &kNodeIDAndReservations : mReservations) {
        for (const auto &kPathIDAndReservation : kNodeIDAndReservations.second) {
            auto equiv = kPathIDAndReservation.second->equivalent();
            topologyCacheManager(equiv)->resetInitiatorCache();
            maxFlowCacheManager(equiv)->clearCashes();
        }
    }

    debug() << "Transaction committed.";
    saveVotes(ioTransaction);
    debug() << "Votes saved.";

    // delete this transaction from storage
    ioTransaction->transactionHandler()->deleteRecordIfExists(
        currentTransactionUUID());
}

void BaseExchangePaymentTransaction::invalidateCachedPathsDueToCommit()
{
    debug() << "invalidateCachedPathsDueToCommit";
    for (const auto &kNodeIDAndReservations : mReservations) {
        for (const auto &kPathIDAndReservation : kNodeIDAndReservations.second) {
            mExchangePathsManager->invalidatePathsForEquivalent(kPathIDAndReservation.second->equivalent());
        }
    }
}

// Marks all trust lines with reservations as Conflict state.
void BaseExchangePaymentTransaction::setTrustLinesToConflictState()
{
    info() << "Setting trust lines with reservations to Conflict state";

    // Iterate over all reservations and mark related trust lines as Conflict.
    for (const auto &nodeAndReservations : mReservations) {
        auto contractorID = nodeAndReservations.first;
        for (const auto &pathIDAndReservation : nodeAndReservations.second) {
            auto equivalent = pathIDAndReservation.second->equivalent();
            auto trustLineManager = trustLinesManager(equivalent);

            debug() << "Setting trust line to Conflict: contractor " << contractorID
                    << ", equivalent " << equivalent;

            trustLineManager->setTrustLineState(
                contractorID,
                TrustLine::Conflict);
        }
    }
}

void BaseExchangePaymentTransaction::rollBack()
{
    debug() << "rollback";
    // drop reservations in AmountReservationHandler
    for (const auto &kNodeIDAndReservations : mReservations) {
        for (const auto &kPathIDAndReservation : kNodeIDAndReservations.second) {
            // Get the appropriate TrustLinesManager for this reservation's equivalent
            auto trustLineManager = trustLinesManager(kPathIDAndReservation.second->equivalent());
            trustLineManager->dropAmountReservation(
                kNodeIDAndReservations.first,
                kPathIDAndReservation.second);

            if (kPathIDAndReservation.second->direction() == AmountReservation::Outgoing)
                debug() << "Dropping reservation: [ => ] " << kPathIDAndReservation.second->amount()
                        << " for (" << kNodeIDAndReservations.first << ") [" << kPathIDAndReservation.first
                        << "] [equiv: " << kPathIDAndReservation.second->equivalent() << "]";

            else if (kPathIDAndReservation.second->direction() == AmountReservation::Incoming)
                debug() << "Dropping reservation: [ <= ] " << kPathIDAndReservation.second->amount()
                        << " for (" << kNodeIDAndReservations.first << ") [" << kPathIDAndReservation.first
                        << "] [equiv: " << kPathIDAndReservation.second->equivalent() << "]";
        }
    }

    // delete transaction references on dropped reservations
    mReservations.clear();
}

void BaseExchangePaymentTransaction::rollBack(const PathID &pathID)
{
    debug() << "rollback on path";

    auto itNodeIDAndReservations = mReservations.begin();
    while (itNodeIDAndReservations != mReservations.end()) {
        auto itPathIDAndReservation = itNodeIDAndReservations->second.begin();
        while (itPathIDAndReservation != itNodeIDAndReservations->second.end()) {
            if (itPathIDAndReservation->first == pathID) {

                // Get the appropriate TrustLinesManager for this reservation's equivalent
                auto trustLineManager = trustLinesManager(itPathIDAndReservation->second->equivalent());
                trustLineManager->dropAmountReservation(
                    itNodeIDAndReservations->first,
                    itPathIDAndReservation->second);

                if (itPathIDAndReservation->second->direction() == AmountReservation::Outgoing)
                    debug() << "Dropping reservation: [ => ] " << itPathIDAndReservation->second->amount()
                            << " for (" << itNodeIDAndReservations->first << ") [" << itPathIDAndReservation->first
                            << "] [equiv: " << itPathIDAndReservation->second->equivalent() << "]";

                else if (itPathIDAndReservation->second->direction() == AmountReservation::Incoming)
                    debug() << "Dropping reservation: [ <= ] " << itPathIDAndReservation->second->amount()
                            << " for (" << itNodeIDAndReservations->first << ") [" << itPathIDAndReservation->first
                            << "] [equiv: " << itPathIDAndReservation->second->equivalent() << "]";

                itPathIDAndReservation = itNodeIDAndReservations->second.erase(itPathIDAndReservation);
            } else {
                itPathIDAndReservation++;
            }
        }
        if (itNodeIDAndReservations->second.empty()) {
            itNodeIDAndReservations = mReservations.erase(itNodeIDAndReservations);
        } else {
            itNodeIDAndReservations++;
        }
    }
}

void BaseExchangePaymentTransaction::removeAllDataFromStorageConcerningTransaction(
    IOTransaction::Shared ioTransaction)
{
    if (ioTransaction == nullptr) {
        ioTransaction = mStorageHandler->beginTransaction();
    }
    ioTransaction->outgoingPaymentReceiptHandler()->deleteRecords(
        mTransactionUUID);
    ioTransaction->incomingPaymentReceiptHandler()->deleteRecords(
        mTransactionUUID);
    ioTransaction->paymentParticipantsVotesHandler()->deleteRecords(
        mTransactionUUID);
    ioTransaction->transactionHandler()->deleteRecordIfExists(
        mTransactionUUID);
}

uint32_t BaseExchangePaymentTransaction::maxNetworkDelay(const uint16_t totalHopsCount) const
{
    return kMaxMessageTransferLagMSec * (uint32_t)(totalHopsCount);
}

const bool BaseExchangePaymentTransaction::contextIsValid(
    Message::MessageType messageType,
    bool showErrorMessage) const
{
    if (mContext.empty()) {
        if (showErrorMessage) {
            warning() << "contextIsValid::context is empty";
        }
        return false;
    }

    if (mContext.size() > 1) {
        if (showErrorMessage) {
            stringstream stream;
            stream << "contextIsValid::context has " << mContext.size() << " messages: ";
            for (auto const &message : mContext) {
                stream << message->typeID() << " ";
            }
            warning() << stream.str();
        }
        return false;
    }

    if (mContext.at(0)->typeID() != messageType) {
        if (showErrorMessage) {
            warning() << "Unexpected message received. (ID " << mContext.at(0)->typeID()
                      << ") It seems that remote node doesn't follows the protocol. Canceling.";
        }

        return false;
    }

    return true;
}

const bool BaseExchangePaymentTransaction::resourceIsValid(
    BaseResource::ResourceType resourceType) const
{
    if (mResources.empty()) {
        warning() << "resources are empty";
        return false;
    }
    if (mResources.at(0)->type() != resourceType) {
        warning() << "Unexpected resource received. (ID " << mResources.at(0)->type() << ") Canceling.";
        return false;
    }

    return true;
}

const bool BaseExchangePaymentTransaction::rpcRequestIsValid(RpcMethod rpcMethod) const
{
    if (mRpcContext.empty()) {
        warning() << "rpc requests are empty";
        return false;
    }
    if (mRpcContext.at(0)->method() != rpcMethod) {
        warning() << "Unexpected rpc responce received.";
        return false;
    }
    return true;
}

void BaseExchangePaymentTransaction::dropNodeReservationsOnPath(PathID pathID)
{
    debug() << "dropNodeReservationsOnPath: " << pathID;

    auto itNodeReservations = mReservations.begin();
    while (itNodeReservations != mReservations.end()) {
        auto itPathIDAndReservation = itNodeReservations->second.begin();
        while (itPathIDAndReservation != itNodeReservations->second.end()) {
            if (itPathIDAndReservation->first == pathID) {

                // Get the appropriate TrustLinesManager for this reservation's equivalent
                auto trustLineManager = trustLinesManager(itPathIDAndReservation->second->equivalent());
                trustLineManager->dropAmountReservation(
                    itNodeReservations->first,
                    itPathIDAndReservation->second);

                if (itPathIDAndReservation->second->direction() == AmountReservation::Outgoing)
                    debug() << "Dropping reservation: [ => ] " << itPathIDAndReservation->second->amount()
                            << " for (" << itNodeReservations->first << ") [" << itPathIDAndReservation->first
                            << "] [equiv: " << itPathIDAndReservation->second->equivalent() << "]";

                else if (itPathIDAndReservation->second->direction() == AmountReservation::Incoming)
                    debug() << "Dropping reservation: [ <= ] " << itPathIDAndReservation->second->amount()
                            << " for (" << itNodeReservations->first << ") [" << itPathIDAndReservation->first
                            << "] [equiv: " << itPathIDAndReservation->second->equivalent() << "]";

                itPathIDAndReservation = itNodeReservations->second.erase(itPathIDAndReservation);
            } else {
                itPathIDAndReservation++;
            }
        }
        if (itNodeReservations->second.empty()) {
            itNodeReservations = mReservations.erase(itNodeReservations);
        } else {
            itNodeReservations++;
        }
    }
}

void BaseExchangePaymentTransaction::runThreeNodesCyclesTransactions()
{
    mBuildCycleThreeNodesSignal(
        mContractorsForCycles,
        mEquivalent);
}

void BaseExchangePaymentTransaction::runFourNodesCyclesTransactions()
{
    mBuildCycleFourNodesSignal(
        mContractorsForCycles,
        mEquivalent);
}

bool BaseExchangePaymentTransaction::updateReservations(
    const vector<PathReservation> &finalAmounts)
{
    debug() << "updateReservations";

    // Track which finalAmounts entries were matched
    unordered_set<size_t> matchedFinalAmounts;
    const auto reservationsCopy = mReservations;
    info() << "reservations size: " << reservationsCopy.size();

    for (const auto &nodeAndReservations : reservationsCopy) {
        for (auto pathIDAndReservation : nodeAndReservations.second) {
            bool found = false;

            // Find matching final amount by BOTH pathID AND equivalent
            // This allows exchange nodes to have multiple reservations with same pathID but different equivalents
            for (size_t i = 0; i < finalAmounts.size(); ++i) {
                const auto &finalAmount = finalAmounts[i];

                // Match requires BOTH pathID and equivalent to be equal
                if (finalAmount.pathID == pathIDAndReservation.first &&
                    finalAmount.equivalent == pathIDAndReservation.second->equivalent() &&
                    finalAmount.direction == pathIDAndReservation.second->direction()) {

                    // Found exact match by (pathID, equivalent) pair
                    debug() << "updateReservations: Found match for PathID " << finalAmount.pathID
                            << ", equivalent " << finalAmount.equivalent
                            << ", amount " << *finalAmount.amount.get()
                            << ", direction " << finalAmount.direction;

                    if (*finalAmount.amount.get() != pathIDAndReservation.second->amount()) {
                        shortageReservation(
                            nodeAndReservations.first,
                            pathIDAndReservation.second,
                            *finalAmount.amount.get(),
                            finalAmount.pathID,
                            finalAmount.equivalent);
                    }

                    matchedFinalAmounts.insert(i);
                    found = true;
                    break;
                }
            }

            if (!found) {
                // Reservation with (pathID, equivalent) not in finalAmounts - drop it
                warning() << "updateReservations: No match found for reservation PathID "
                          << pathIDAndReservation.first
                          << ", equivalent " << pathIDAndReservation.second->equivalent()
                          << " - dropping";
                dropNodeReservationsOnPath(pathIDAndReservation.first);
            }
        }
    }

    // Verify all finalAmounts were matched to existing reservations
    if (matchedFinalAmounts.size() != finalAmounts.size()) {
        warning() << "updateReservations: Not all finalAmounts were matched. Expected: "
                  << finalAmounts.size() << ", matched: " << matchedFinalAmounts.size();
        return false;
    }

    return true;
}

PathID BaseExchangePaymentTransaction::updateReservation(
    ContractorID contractorID,
    pair<PathID, AmountReservation::ConstShared> &pathIDAndReservation,
    const vector<PathReservation> &finalAmounts)
{
    for (auto &pathReservation : finalAmounts) {
        if (pathReservation.pathID == pathIDAndReservation.first) {
            if (*pathReservation.amount.get() != pathIDAndReservation.second->amount()) {
                shortageReservation(
                    contractorID,
                    pathIDAndReservation.second,
                    *pathReservation.amount.get(),
                    pathReservation.pathID,
                    pathReservation.equivalent);
            }
            return pathReservation.pathID;
        }
    }
    dropNodeReservationsOnPath(
        pathIDAndReservation.first);
    return std::numeric_limits<PathID>::max();
}

size_t BaseExchangePaymentTransaction::reservationsSizeInBytes() const
{
    size_t reservationSizeInBytes = sizeof(uint32_t); // Magic number
    for (const auto &nodeAndReservations : mReservations) {
        reservationSizeInBytes += sizeof(ContractorID) + nodeAndReservations.second.size() * (sizeof(PathID) +             // PathID
                                  kTrustLineAmountBytesCount + // Reservation Amount
                                  sizeof(AmountReservation::SerializedReservationDirectionSize) + // Reservation Direction
                                  sizeof(SerializedEquivalent)) + // Reservation Equivalent
                                  sizeof(SerializedRecordsCount); // Vector Size
    }
    reservationSizeInBytes += sizeof(SerializedRecordsCount); // map Size
    return reservationSizeInBytes;
}

const TrustLineAmount BaseExchangePaymentTransaction::totalReservedAmount(
    AmountReservation::ReservationDirection reservationDirection,
    const SerializedEquivalent equivalent) const
{
    TrustLineAmount totalAmount = 0;
    for (const auto &nodeIDAndReservations : mReservations) {
        for (const auto &pathIDAndReservation : nodeIDAndReservations.second) {
            if (pathIDAndReservation.second->direction() == reservationDirection &&
                pathIDAndReservation.second->equivalent() == equivalent) {
                totalAmount += pathIDAndReservation.second->amount();
            }
        }
    }
    return totalAmount;
}

pair<BytesShared, size_t> BaseExchangePaymentTransaction::getSerializedReceipt(
    ContractorID source,
    ContractorID target,
    const TrustLineAmount &amount,
    bool isSource,
    const SerializedEquivalent equivalent)
{
    size_t serializedDataSize = sizeof(ContractorID) + sizeof(ContractorID) + sizeof(BlockNumber) + TransactionUUID::kBytesSize + kTrustLineAmountBytesCount + sizeof(AuditNumber);
    BytesShared serializedData = tryMalloc(serializedDataSize);

    size_t bytesBufferOffset = 0;
    memcpy(
        serializedData.get() + bytesBufferOffset,
        &source,
        sizeof(ContractorID));
    bytesBufferOffset += sizeof(ContractorID);

    memcpy(
        serializedData.get() + bytesBufferOffset,
        &target,
        sizeof(ContractorID));
    bytesBufferOffset += sizeof(ContractorID);

    memcpy(
        serializedData.get() + bytesBufferOffset,
        &mMaximalClaimingBlockNumber,
        sizeof(BlockNumber));
    bytesBufferOffset += sizeof(BlockNumber);

    memcpy(
        serializedData.get() + bytesBufferOffset,
        mTransactionUUID.data,
        TransactionUUID::kBytesSize);
    bytesBufferOffset += TransactionUUID::kBytesSize;

    auto serializedAmount = trustLineAmountToBytes(amount);
    memcpy(
        serializedData.get() + bytesBufferOffset,
        serializedAmount.data(),
        kTrustLineAmountBytesCount);
    bytesBufferOffset += kTrustLineAmountBytesCount;

    AuditNumber currentAuditNumber;
    auto trustLineManager = trustLinesManager(equivalent);
    if (isSource) {
        currentAuditNumber = trustLineManager->auditNumber(target);
    } else {
        currentAuditNumber = trustLineManager->auditNumber(source);
    }
    memcpy(
        serializedData.get() + bytesBufferOffset,
        &currentAuditNumber,
        sizeof(AuditNumber));

    debug() << "Receipt " << source << " " << target << " " << amount << " "
            << mMaximalClaimingBlockNumber << " " << currentAuditNumber << " [equiv: " << equivalent << "]";

    return make_pair(
               serializedData,
               serializedDataSize);
}

bool BaseExchangePaymentTransaction::checkAllNeighborsWithReservationsAreInFinalParticipantsList()
{
    for (const auto &nodeAndReservations : mReservations) {
        auto contractorAddress = mContractorsManager->contractorMainAddress(nodeAndReservations.first);
        if (mPaymentNodesIds.find(contractorAddress->fullAddress()) == mPaymentNodesIds.end()) {
            return false;
        }
    }
    return true;
}

bool BaseExchangePaymentTransaction::checkAllPublicKeyHashesProperly()
{
    for (const auto &paymentNodeIdAndContractor : mPaymentParticipants) {
        if (paymentNodeIdAndContractor.first == kCoordinatorPaymentNodeID) {
            continue;
        }
        if (mParticipantsPublicKeysHashes.find(paymentNodeIdAndContractor.second->mainAddress()->fullAddress()) ==
                mParticipantsPublicKeysHashes.end()) {
            warning() << "Public key hash of " << paymentNodeIdAndContractor.second->mainAddress()->fullAddress() << " is absent";
            return false;
        }
        if (mParticipantsPublicKeysHashes[paymentNodeIdAndContractor.second->mainAddress()->fullAddress()].first !=
                paymentNodeIdAndContractor.first) {
            warning() << "Invalid Payment node ID of " << paymentNodeIdAndContractor.second->mainAddress()->fullAddress();
            return false;
        }
    }
    return true;
}

const TrustLineAmount BaseExchangePaymentTransaction::totalReservedIncomingAmountToNode(
    ContractorID contractorID,
    const SerializedEquivalent equivalent) const
{
    auto result = TrustLine::kZeroAmount();
    if (mReservations.find(contractorID) == mReservations.end()) {
        return result;
    }
    for (const auto &pathIDAndReservation : mReservations.at(contractorID)) {
        if (pathIDAndReservation.second->direction() == AmountReservation::Incoming &&
            pathIDAndReservation.second->equivalent() == equivalent) {
            result += pathIDAndReservation.second->amount();
        }
    }
    return result;
}

bool BaseExchangePaymentTransaction::checkPublicKeysAppropriate()
{
    // coordinator don't send own hash, because it send own public key directly
    if (mParticipantsPublicKeys.size() != mParticipantsPublicKeysHashes.size() + 1) {
        warning() << "different numbers of public keys and public keys hashes";
        return false;
    }
    for (const auto &nodeAndPaymentNodeID : mParticipantsPublicKeysHashes) {
        if (mParticipantsPublicKeys.find(nodeAndPaymentNodeID.second.first) == mParticipantsPublicKeys.end()) {
            warning() << "public key from node " << nodeAndPaymentNodeID.first << " ["
                      << nodeAndPaymentNodeID.second.first << "] is absent";
            return false;
        }
        // In SPHINCS+ single-key architecture, no hash verification needed
    }
    return true;
}

pair<BytesShared, size_t> BaseExchangePaymentTransaction::getSerializedParticipantsVotesData(
    Contractor::Shared contractor)
{
    size_t serializedDataSize = sizeof(BlockNumber) + TransactionUUID::kBytesSize + sizeof(SerializedRecordsCount) 
                              + mParticipantsPublicKeys.size() * sphincs::PublicKey::keySize();
    BytesShared serializedData = tryMalloc(serializedDataSize);

    size_t bytesBufferOffset = 0;
    memcpy(
        serializedData.get() + bytesBufferOffset,
        &mMaximalClaimingBlockNumber,
        sizeof(BlockNumber));
    bytesBufferOffset += sizeof(BlockNumber);

    memcpy(
        serializedData.get() + bytesBufferOffset,
        mTransactionUUID.data,
        TransactionUUID::kBytesSize);
    bytesBufferOffset += TransactionUUID::kBytesSize;

    auto participantsCount = mParticipantsPublicKeys.size();
    memcpy(
        serializedData.get() + bytesBufferOffset,
        &participantsCount,
        sizeof(SerializedRecordsCount));
    bytesBufferOffset += sizeof(SerializedRecordsCount);

    for (const auto &paymentNodeIdAndPublicKey : mParticipantsPublicKeys) {
        memcpy(
            serializedData.get() + bytesBufferOffset,
            paymentNodeIdAndPublicKey.second->data(),
            sphincs::PublicKey::keySize());
        bytesBufferOffset += sphincs::PublicKey::keySize();
    }

    return make_pair(
               serializedData,
               serializedDataSize);
}

bool BaseExchangePaymentTransaction::checkSignaturesAppropriate()
{
    if (mParticipantsSignatures.size() != mParticipantsPublicKeys.size()) {
        warning() << "different numbers of signatures and participants";
        return false;
    }
    for (const auto &paymentNodeIdAndPubKey : mParticipantsPublicKeys) {
        if (mParticipantsSignatures.find(paymentNodeIdAndPubKey.first) == mParticipantsSignatures.end()) {
            warning() << "there are no signature from participant " << paymentNodeIdAndPubKey.first;
        }
    }
    return true;
}

bool BaseExchangePaymentTransaction::checkMaxClaimingBlockNumber(
    BlockNumber maxClaimingBlockNumberOnOwnSide)
{
    if (mMaximalClaimingBlockNumber > maxClaimingBlockNumberOnOwnSide) {
        return mMaximalClaimingBlockNumber - maxClaimingBlockNumberOnOwnSide <= kAllowableBlockNumberDifference;
    }
    if (mMaximalClaimingBlockNumber < maxClaimingBlockNumberOnOwnSide) {
        return maxClaimingBlockNumberOnOwnSide - mMaximalClaimingBlockNumber <= kAllowableBlockNumberDifference;
    }
    return true;
}
