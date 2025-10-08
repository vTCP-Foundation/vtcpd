#include "BaseExchangePaymentTransaction.h"

BaseExchangePaymentTransaction::BaseExchangePaymentTransaction(
    const TransactionType type,
    const SerializedEquivalent equivalent,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    StorageHandler *storageHandler,
    ResourcesManager *resourcesManager,
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

    // Check if this is new format (with magic number) or old format (without)
    const uint32_t kReservationsFormatMagic = 0x52455356; // "RESV" in ASCII
    uint32_t possibleMagic;
    memcpy(
        &possibleMagic,
        buffer.get() + bytesBufferOffset,
        sizeof(uint32_t));

    bool isNewFormat = (possibleMagic == kReservationsFormatMagic);

    SerializedRecordsCount reservationsCount;
    if (isNewFormat) {
        // New format: skip magic number and read count
        bytesBufferOffset += sizeof(uint32_t);
        memcpy(
            &reservationsCount,
            buffer.get() + bytesBufferOffset,
            sizeof(SerializedRecordsCount));
        bytesBufferOffset += sizeof(SerializedRecordsCount);
    } else {
        // Old format: first 4 bytes are the count (no magic number)
        // This should not happen for BaseExchangePaymentTransaction as it's a new class,
        // but we support it for consistency
        reservationsCount = possibleMagic; // reuse already read value
        bytesBufferOffset += sizeof(SerializedRecordsCount);
    }

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

            // Equivalent (critical for multi-equivalent support)
            SerializedEquivalent stepEquivalent;
            if (isNewFormat) {
                // New format: read equivalent from buffer
                memcpy(
                    &stepEquivalent,
                    buffer.get() + bytesBufferOffset,
                    sizeof(SerializedEquivalent));
                bytesBufferOffset += sizeof(SerializedEquivalent);
            } else {
                // Old format: this shouldn't normally happen for BaseExchangePaymentTransaction,
                // but if it does, we need a default equivalent. Use the transaction's equivalent.
                stepEquivalent = mEquivalent;
                // DO NOT advance bytesBufferOffset - there's no equivalent in old format
            }

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

    // Note: Recovery stage setting would be done in derived classes if needed
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

    // Reservation Part
    // Write magic number to indicate new format with equivalents
    const uint32_t kReservationsFormatMagic = 0x52455356; // "RESV" in ASCII
    memcpy(
        dataBytesShared.get() + dataBytesOffset,
        &kReservationsFormatMagic,
        sizeof(uint32_t));
    dataBytesOffset += sizeof(uint32_t);

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
    throw RuntimeError("BaseExchangePaymentTransaction::reserveOutgoingAmount not yet implemented");
}

const bool BaseExchangePaymentTransaction::reserveIncomingAmount(
    ContractorID neighborNode,
    const TrustLineAmount& amount,
    const PathID &pathID,
    const SerializedEquivalent equivalent)
{
    throw RuntimeError("BaseExchangePaymentTransaction::reserveIncomingAmount not yet implemented");
}

const bool BaseExchangePaymentTransaction::copyReservationFromGlobalReservations(
    ContractorID neighborNode,
    const TrustLineAmount& amount,
    AmountReservation::ReservationDirection reservationDirection,
    const PathID &pathID,
    const SerializedEquivalent equivalent)
{
    throw RuntimeError("BaseExchangePaymentTransaction::copyReservationFromGlobalReservations not yet implemented");
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
    throw RuntimeError("BaseExchangePaymentTransaction::runVotesCheckingStage not yet implemented");
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::runVotesConsistencyCheckingStage()
{
    throw RuntimeError("BaseExchangePaymentTransaction::runVotesConsistencyCheckingStage not yet implemented");
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::processParticipantsVotesMessage()
{
    throw RuntimeError("BaseExchangePaymentTransaction::processParticipantsVotesMessage not yet implemented");
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::approve()
{
    throw RuntimeError("BaseExchangePaymentTransaction::approve not yet implemented");
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::recover(const char* message)
{
    throw RuntimeError("BaseExchangePaymentTransaction::recover not yet implemented");
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::reject(const char* message)
{
    throw RuntimeError("BaseExchangePaymentTransaction::reject not yet implemented");
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::runVotesRecoveryParentStage()
{
    throw RuntimeError("BaseExchangePaymentTransaction::runVotesRecoveryParentStage not yet implemented");
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::sendVotesRequestMessageAndWaitForResponse(
    Contractor::Shared contractor)
{
    throw RuntimeError("BaseExchangePaymentTransaction::sendVotesRequestMessageAndWaitForResponse not yet implemented");
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::processNextNodeToCheckVotes()
{
    throw RuntimeError("BaseExchangePaymentTransaction::processNextNodeToCheckVotes not yet implemented");
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::runPrepareListNodesToCheckNodes()
{
    throw RuntimeError("BaseExchangePaymentTransaction::runPrepareListNodesToCheckNodes not yet implemented");
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::runCheckCoordinatorVotesStage()
{
    throw RuntimeError("BaseExchangePaymentTransaction::runCheckCoordinatorVotesStage not yet implemented");
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::runCheckIntermediateNodeVotesStage()
{
    throw RuntimeError("BaseExchangePaymentTransaction::runCheckIntermediateNodeVotesStage not yet implemented");
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::runObservingStage()
{
    throw RuntimeError("BaseExchangePaymentTransaction::runObservingStage not yet implemented");
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::runObservingResultStage()
{
    throw RuntimeError("BaseExchangePaymentTransaction::runObservingResultStage not yet implemented");
}

TransactionResult::SharedConst BaseExchangePaymentTransaction::runObservingRejectTransaction()
{
    throw RuntimeError("BaseExchangePaymentTransaction::runObservingRejectTransaction not yet implemented");
}

void BaseExchangePaymentTransaction::saveVotes(IOTransaction::Shared ioTransaction)
{
    throw RuntimeError("BaseExchangePaymentTransaction::saveVotes not yet implemented");
}

void BaseExchangePaymentTransaction::commit(IOTransaction::Shared ioTransaction)
{
    throw RuntimeError("BaseExchangePaymentTransaction::commit not yet implemented");
}

void BaseExchangePaymentTransaction::rollBack()
{
    throw RuntimeError("BaseExchangePaymentTransaction::rollBack not yet implemented");
}

void BaseExchangePaymentTransaction::rollBack(const PathID &pathID)
{
    throw RuntimeError("BaseExchangePaymentTransaction::rollBack(pathID) not yet implemented");
}

void BaseExchangePaymentTransaction::removeAllDataFromStorageConcerningTransaction(
    IOTransaction::Shared ioTransaction)
{
    throw RuntimeError("BaseExchangePaymentTransaction::removeAllDataFromStorageConcerningTransaction not yet implemented");
}

uint32_t BaseExchangePaymentTransaction::maxNetworkDelay(const uint16_t totalHopsCount) const
{
    return kMaxMessageTransferLagMSec * (uint32_t)(totalHopsCount);
}

const bool BaseExchangePaymentTransaction::contextIsValid(
    Message::MessageType messageType,
    bool showErrorMessage) const
{
    throw RuntimeError("BaseExchangePaymentTransaction::contextIsValid not yet implemented");
}

const bool BaseExchangePaymentTransaction::resourceIsValid(
    BaseResource::ResourceType resourceType) const
{
    throw RuntimeError("BaseExchangePaymentTransaction::resourceIsValid not yet implemented");
}

void BaseExchangePaymentTransaction::dropNodeReservationsOnPath(PathID pathID)
{
    throw RuntimeError("BaseExchangePaymentTransaction::dropNodeReservationsOnPath not yet implemented");
}

void BaseExchangePaymentTransaction::runThreeNodesCyclesTransactions()
{
    throw RuntimeError("BaseExchangePaymentTransaction::runThreeNodesCyclesTransactions not yet implemented");
}

void BaseExchangePaymentTransaction::runFourNodesCyclesTransactions()
{
    throw RuntimeError("BaseExchangePaymentTransaction::runFourNodesCyclesTransactions not yet implemented");
}

bool BaseExchangePaymentTransaction::updateReservations(
    const vector<PathReservation> &finalAmounts)
{
    throw RuntimeError("BaseExchangePaymentTransaction::updateReservations not yet implemented");
}

PathID BaseExchangePaymentTransaction::updateReservation(
    ContractorID contractorID,
    pair<PathID, AmountReservation::ConstShared> &pathIDAndReservation,
    const vector<PathReservation> &finalAmounts)
{
    throw RuntimeError("BaseExchangePaymentTransaction::updateReservation not yet implemented");
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
    bool isSource)
{
    throw RuntimeError("BaseExchangePaymentTransaction::getSerializedReceipt not yet implemented");
}

bool BaseExchangePaymentTransaction::checkAllNeighborsWithReservationsAreInFinalParticipantsList()
{
    throw RuntimeError("BaseExchangePaymentTransaction::checkAllNeighborsWithReservationsAreInFinalParticipantsList not yet implemented");
}

bool BaseExchangePaymentTransaction::checkAllPublicKeyHashesProperly()
{
    throw RuntimeError("BaseExchangePaymentTransaction::checkAllPublicKeyHashesProperly not yet implemented");
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
    throw RuntimeError("BaseExchangePaymentTransaction::checkPublicKeysAppropriate not yet implemented");
}

pair<BytesShared, size_t> BaseExchangePaymentTransaction::getSerializedParticipantsVotesData(
    Contractor::Shared contractor)
{
    throw RuntimeError("BaseExchangePaymentTransaction::getSerializedParticipantsVotesData not yet implemented");
}

bool BaseExchangePaymentTransaction::checkSignaturesAppropriate()
{
    throw RuntimeError("BaseExchangePaymentTransaction::checkSignaturesAppropriate not yet implemented");
}

bool BaseExchangePaymentTransaction::checkMaxClaimingBlockNumber(
    BlockNumber maxClaimingBlockNumberOnOwnSide)
{
    throw RuntimeError("BaseExchangePaymentTransaction::checkMaxClaimingBlockNumber not yet implemented");
}
