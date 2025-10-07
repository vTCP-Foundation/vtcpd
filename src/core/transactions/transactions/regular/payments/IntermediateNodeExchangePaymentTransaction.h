#ifndef VTCPD_INTERMEDIATENODEEXCHANGEPAYMENTTRANSACTION_H
#define VTCPD_INTERMEDIATENODEEXCHANGEPAYMENTTRANSACTION_H

#include "base/BaseExchangePaymentTransaction.h"
#include "../../../common/exceptions/RuntimeError.h"

class IntermediateNodeExchangePaymentTransaction : public BaseExchangePaymentTransaction
{

public:
    typedef shared_ptr<IntermediateNodeExchangePaymentTransaction> Shared;

public:
    IntermediateNodeExchangePaymentTransaction(
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        StorageHandler *storageHandler,
        ResourcesManager *resourcesManager,
        Keystore *keystore,
        Logger &log,
        SubsystemsController *subsystemsController);

    IntermediateNodeExchangePaymentTransaction(
        BytesShared buffer,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        StorageHandler *storageHandler,
        ResourcesManager *resourcesManager,
        Keystore *keystore,
        Logger &log,
        SubsystemsController *subsystemsController);

    TransactionResult::SharedConst run() override;

protected:
    TransactionResult::SharedConst runPreviousNeighborRequestProcessingStage();
    TransactionResult::SharedConst runCoordinatorRequestProcessingStage();
    TransactionResult::SharedConst runNextNeighborResponseProcessingStage();
    TransactionResult::SharedConst runReservationProlongationStage();
    TransactionResult::SharedConst runVotesConsistencyCheckingStage() override;

    void savePaymentOperationIntoHistory(IOTransaction::Shared ioTransaction) override;
    bool checkReservationsDirections() const override;
};

#endif //VTCPD_INTERMEDIATENODEEXCHANGEPAYMENTTRANSACTION_H
