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
    mPayload("")
{
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

// Placeholder implementations for methods (to be implemented in subsequent tasks)
pair<BytesShared, size_t> BaseExchangePaymentTransaction::serializeToBytes() const
{
    throw RuntimeError("BaseExchangePaymentTransaction::serializeToBytes not yet implemented");
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
    throw RuntimeError("BaseExchangePaymentTransaction::shortageReservation not yet implemented");
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
    throw RuntimeError("BaseExchangePaymentTransaction::reservationsSizeInBytes not yet implemented");
}

const TrustLineAmount BaseExchangePaymentTransaction::totalReservedAmount(
    AmountReservation::ReservationDirection reservationDirection,
    const SerializedEquivalent equivalent) const
{
    throw RuntimeError("BaseExchangePaymentTransaction::totalReservedAmount not yet implemented");
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
    throw RuntimeError("BaseExchangePaymentTransaction::totalReservedIncomingAmountToNode not yet implemented");
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
