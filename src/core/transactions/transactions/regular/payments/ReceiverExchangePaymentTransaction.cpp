#include "ReceiverExchangePaymentTransaction.h"

ReceiverExchangePaymentTransaction::ReceiverExchangePaymentTransaction(
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    StorageHandler *storageHandler,
    ResourcesManager *resourcesManager,
    Keystore *keystore,
    Logger &log,
    SubsystemsController *subsystemsController) :

    BaseExchangePaymentTransaction(
        BaseTransaction::ReceiverPaymentTransaction,
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

ReceiverExchangePaymentTransaction::ReceiverExchangePaymentTransaction(
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

TransactionResult::SharedConst ReceiverExchangePaymentTransaction::run()
{
    throw RuntimeError("ReceiverExchangePaymentTransaction::run not yet implemented");
}

TransactionResult::SharedConst ReceiverExchangePaymentTransaction::runApproveCoordinatorRequestStage()
{
    throw RuntimeError("ReceiverExchangePaymentTransaction::runApproveCoordinatorRequestStage not yet implemented");
}

TransactionResult::SharedConst ReceiverExchangePaymentTransaction::runAmountReservationStage()
{
    throw RuntimeError("ReceiverExchangePaymentTransaction::runAmountReservationStage not yet implemented");
}

TransactionResult::SharedConst ReceiverExchangePaymentTransaction::runVotesConsistencyCheckingStage()
{
    throw RuntimeError("ReceiverExchangePaymentTransaction::runVotesConsistencyCheckingStage not yet implemented");
}

void ReceiverExchangePaymentTransaction::savePaymentOperationIntoHistory(
    IOTransaction::Shared ioTransaction)
{
    throw RuntimeError("ReceiverExchangePaymentTransaction::savePaymentOperationIntoHistory not yet implemented");
}

bool ReceiverExchangePaymentTransaction::checkReservationsDirections() const
{
    throw RuntimeError("ReceiverExchangePaymentTransaction::checkReservationsDirections not yet implemented");
}
