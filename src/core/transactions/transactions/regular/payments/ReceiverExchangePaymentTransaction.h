#ifndef VTCPD_RECEIVEREXCHANGEPAYMENTTRANSACTION_H
#define VTCPD_RECEIVEREXCHANGEPAYMENTTRANSACTION_H

#include "base/BaseExchangePaymentTransaction.h"
#include "../../../common/exceptions/RuntimeError.h"
#include "../../../rates/manager/ExchangeRatesManager.h"
#include "../../../rates/manager/CommissionsManager.h"
#include "../../../interface/events_interface/interface/EventsInterfaceManager.h"
#include "../../../contractors/Contractor.h"

class ReceiverExchangePaymentTransaction : public BaseExchangePaymentTransaction
{

public:
    typedef shared_ptr<ReceiverExchangePaymentTransaction> Shared;

public:
    ReceiverExchangePaymentTransaction(
        ReceiverInitPaymentRequestMessage::ConstShared message,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        StorageHandler *storageHandler,
        ResourcesManager *resourcesManager,
        ExchangeRatesManager *exchangeRatesManager,
        CommissionsManager *commissionsManager,
        Keystore *keystore,
        EventsInterfaceManager *eventsInterfaceManager,
        Logger &log,
        SubsystemsController *subsystemsController);

    ReceiverExchangePaymentTransaction(
        BytesShared buffer,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        StorageHandler *storageHandler,
        ResourcesManager *resourcesManager,
        ExchangeRatesManager *exchangeRatesManager,
        CommissionsManager *commissionsManager,
        Keystore *keystore,
        EventsInterfaceManager *eventsInterfaceManager,
        Logger &log,
        SubsystemsController *subsystemsController);

    TransactionResult::SharedConst run() override;

    BaseAddress::Shared coordinatorAddress() const override;

    const string logHeader() const override;

protected:
    TransactionResult::SharedConst runInitializationStage();
    TransactionResult::SharedConst runAmountReservationStage();
    TransactionResult::SharedConst runFinalAmountsConfigurationConfirmation();
    TransactionResult::SharedConst runFinalReservationsCoordinatorConfirmation();
    TransactionResult::SharedConst runFinalReservationsNeighborConfirmation();
    TransactionResult::SharedConst runCheckObservingBlockNumber();
    TransactionResult::SharedConst runVotesStageWithCoordinatorClarification();

    TransactionResult::SharedConst approve() override;
    void savePaymentOperationIntoHistory(IOTransaction::Shared ioTransaction) override;
    bool checkReservationsDirections() const override;

    TransactionResult::SharedConst sendErrorMessageOnPreviousNodeRequest(
        BaseAddress::Shared previousNode,
        PathID pathID,
        ResponseMessage::OperationState errorState);

    void sendErrorMessageOnFinalAmountsConfiguration();

private:
    ExchangeRatesManager *mExchangeRatesManager;
    CommissionsManager *mCommissionsManager;

    EventsInterfaceManager *mEventsInterfaceManager;
    Contractor::Shared mCoordinator;

    TrustLineAmount mTransactionAmount;

    // this field indicates that transaction should be rejected on voting stage
    bool mTransactionShouldBeRejected;
};

#endif //VTCPD_RECEIVEREXCHANGEPAYMENTTRANSACTION_H
