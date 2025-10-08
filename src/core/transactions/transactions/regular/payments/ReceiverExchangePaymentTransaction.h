#ifndef VTCPD_RECEIVEREXCHANGEPAYMENTTRANSACTION_H
#define VTCPD_RECEIVEREXCHANGEPAYMENTTRANSACTION_H

#include "base/BaseExchangePaymentTransaction.h"
#include "../../../common/exceptions/RuntimeError.h"
#include "../../../rates/manager/ExchangeRatesManager.h"
#include "../../../rates/manager/CommissionsManager.h"

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
        ExchangeRatesManager *exchangeRatesManager,
        CommissionsManager *commissionsManager,
        Keystore *keystore,
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
        Logger &log,
        SubsystemsController *subsystemsController);

    TransactionResult::SharedConst run() override;

protected:
    TransactionResult::SharedConst runApproveCoordinatorRequestStage();
    TransactionResult::SharedConst runAmountReservationStage();
    TransactionResult::SharedConst runVotesConsistencyCheckingStage() override;

    void savePaymentOperationIntoHistory(IOTransaction::Shared ioTransaction) override;
    bool updateReservations(const vector<PathReservation> &finalAmounts);
    bool checkReservationsDirections() const override;

private:
    ExchangeRatesManager *mExchangeRatesManager;
    CommissionsManager *mCommissionsManager;

    // Mapping PathID -> SerializedEquivalent for validation
    map<PathID, SerializedEquivalent> mPathEquivalents;
};

#endif //VTCPD_RECEIVEREXCHANGEPAYMENTTRANSACTION_H
