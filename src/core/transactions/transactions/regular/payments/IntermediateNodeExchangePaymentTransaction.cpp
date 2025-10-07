#include "IntermediateNodeExchangePaymentTransaction.h"

IntermediateNodeExchangePaymentTransaction::IntermediateNodeExchangePaymentTransaction(
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    StorageHandler *storageHandler,
    ResourcesManager *resourcesManager,
    Keystore *keystore,
    Logger &log,
    SubsystemsController *subsystemsController) :

    BaseExchangePaymentTransaction(
        BaseTransaction::IntermediateNodePaymentTransaction,
        0, // equivalent will be set from message
        contractorsManager,
        equivalentsSubsystemsRouter,
        storageHandler,
        resourcesManager,
        keystore,
        log,
        subsystemsController)
{
}

IntermediateNodeExchangePaymentTransaction::IntermediateNodeExchangePaymentTransaction(
    BytesShared buffer,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    StorageHandler *storageHandler,
    ResourcesManager *resourcesManager,
    Keystore *keystore,
    Logger &log,
    SubsystemsController *subsystemsController) :

    BaseExchangePaymentTransaction(
        buffer,
        contractorsManager,
        equivalentsSubsystemsRouter,
        storageHandler,
        resourcesManager,
        keystore,
        log,
        subsystemsController)
{
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::run()
{
    throw RuntimeError("IntermediateNodeExchangePaymentTransaction::run not yet implemented");
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::runPreviousNeighborRequestProcessingStage()
{
    throw RuntimeError("IntermediateNodeExchangePaymentTransaction::runPreviousNeighborRequestProcessingStage not yet implemented");
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::runCoordinatorRequestProcessingStage()
{
    throw RuntimeError("IntermediateNodeExchangePaymentTransaction::runCoordinatorRequestProcessingStage not yet implemented");
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::runNextNeighborResponseProcessingStage()
{
    throw RuntimeError("IntermediateNodeExchangePaymentTransaction::runNextNeighborResponseProcessingStage not yet implemented");
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::runReservationProlongationStage()
{
    throw RuntimeError("IntermediateNodeExchangePaymentTransaction::runReservationProlongationStage not yet implemented");
}

TransactionResult::SharedConst IntermediateNodeExchangePaymentTransaction::runVotesConsistencyCheckingStage()
{
    throw RuntimeError("IntermediateNodeExchangePaymentTransaction::runVotesConsistencyCheckingStage not yet implemented");
}

void IntermediateNodeExchangePaymentTransaction::savePaymentOperationIntoHistory(
    IOTransaction::Shared ioTransaction)
{
    throw RuntimeError("IntermediateNodeExchangePaymentTransaction::savePaymentOperationIntoHistory not yet implemented");
}

bool IntermediateNodeExchangePaymentTransaction::checkReservationsDirections() const
{
    throw RuntimeError("IntermediateNodeExchangePaymentTransaction::checkReservationsDirections not yet implemented");
}
