#ifndef VTCPD_COORDINATOREXCHANGEPAYMENTTRANSACTION_H
#define VTCPD_COORDINATOREXCHANGEPAYMENTTRANSACTION_H

#include "base/BaseExchangePaymentTransaction.h"
#include "../../../paths/ExchangePathsManager.h"
#include "../../../interface/events_interface/interface/EventsInterfaceManager.h"
#include "../../../interface/commands_interface/commands/payments/CreditUsageExchangeCommand.h"
#include "../../../paths/lib/OptimalPathResult.h"
#include "base/PathReservation.h"
#include "../../../common/exceptions/RuntimeError.h"

#include <unordered_map>

class CoordinatorExchangePaymentTransaction : public BaseExchangePaymentTransaction
{

public:
    typedef shared_ptr<CoordinatorExchangePaymentTransaction> Shared;
    typedef shared_ptr<const CoordinatorExchangePaymentTransaction> ConstShared;

public:
    CoordinatorExchangePaymentTransaction(
        const CreditUsageExchangeCommand::Shared command,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        StorageHandler *storageHandler,
        ResourcesManager *resourcesManager,
        ExchangePathsManager *exchangePathsManager,
        Keystore *keystore,
        bool isPaymentTransactionsAllowedDueToObserving,
        EventsInterfaceManager *eventsInterfaceManager,
        Logger &log,
        SubsystemsController *subsystemsController);

    TransactionResult::SharedConst run() override;

    const CommandUUID &commandUUID() const;

protected:
    // Stage handlers
    TransactionResult::SharedConst runPaymentInitializationStage();
    TransactionResult::SharedConst runPathsResourceProcessingStage();
    TransactionResult::SharedConst runReceiverRequestProcessingStage();
    TransactionResult::SharedConst runReceiverResponseProcessingStage();
    TransactionResult::SharedConst runAmountReservationStage();
    TransactionResult::SharedConst runDirectAmountReservationResponseProcessingStage();
    TransactionResult::SharedConst runFinalAmountsConfigurationConfirmation();
    TransactionResult::SharedConst runVotesConsistencyCheckingStage() override;
    TransactionResult::SharedConst runTTLTransactionResponse();

    void savePaymentOperationIntoHistory(IOTransaction::Shared ioTransaction) override;
    bool checkReservationsDirections() const override;

    void addPathForFurtherProcessing(const OptimalPathResult& pathResult);

protected:
    ExchangePathsManager *mExchangePathsManager;
    map<PathID, unique_ptr<OptimalPathResult>> mPathsStats;
    vector<SerializedEquivalent> mExchangeEquivalents;

    TrustLineAmount mAmount;
    CommandUUID mCommandUUID;
    ContractorID mContractorID;
    vector<BaseAddress::Shared> mContractorAddresses;
    EventsInterfaceManager *mEventsInterfaceManager;
    bool mIsPaymentTransactionsAllowedDueToObserving;

    vector<PathReservation> mNodesFinalAmountsConfiguration;
};

#endif //VTCPD_COORDINATOREXCHANGEPAYMENTTRANSACTION_H
