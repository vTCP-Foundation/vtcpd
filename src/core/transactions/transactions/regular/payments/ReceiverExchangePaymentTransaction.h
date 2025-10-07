#ifndef VTCPD_RECEIVEREXCHANGEPAYMENTTRANSACTION_H
#define VTCPD_RECEIVEREXCHANGEPAYMENTTRANSACTION_H

#include "base/BaseExchangePaymentTransaction.h"
#include "../../../common/exceptions/RuntimeError.h"

class ReceiverExchangePaymentTransaction : public BaseExchangePaymentTransaction
{

public:
    typedef shared_ptr<ReceiverExchangePaymentTransaction> Shared;

public:
    ReceiverExchangePaymentTransaction(
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        StorageHandler *storageHandler,
        ResourcesManager *resourcesManager,
        Keystore *keystore,
        Logger &log,
        SubsystemsController *subsystemsController);

    ReceiverExchangePaymentTransaction(
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
    TransactionResult::SharedConst runApproveCoordinatorRequestStage();
    TransactionResult::SharedConst runAmountReservationStage();
    TransactionResult::SharedConst runVotesConsistencyCheckingStage() override;

    void savePaymentOperationIntoHistory(IOTransaction::Shared ioTransaction) override;
    bool checkReservationsDirections() const override;
};

#endif //VTCPD_RECEIVEREXCHANGEPAYMENTTRANSACTION_H
