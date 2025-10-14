#ifndef VTCPD_INTERMEDIATENODEEXCHANGEPAYMENTTRANSACTION_H
#define VTCPD_INTERMEDIATENODEEXCHANGEPAYMENTTRANSACTION_H

#include "base/BaseExchangePaymentTransaction.h"
#include "../../../common/exceptions/RuntimeError.h"
#include "../../../rates/manager/ExchangeRatesManager.h"
#include "../../../rates/manager/CommissionsManager.h"

class IntermediateNodeExchangePaymentTransaction : public BaseExchangePaymentTransaction
{

public:
    typedef shared_ptr<IntermediateNodeExchangePaymentTransaction> Shared;

public:
    IntermediateNodeExchangePaymentTransaction(
        IntermediateNodeReservationRequestMessage::ConstShared message,
        ContractorsManager *contractorsManager,
        EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
        StorageHandler *storageHandler,
        ResourcesManager *resourcesManager,
        ExchangeRatesManager *exchangeRatesManager,
        CommissionsManager *commissionsManager,
        Keystore *keystore,
        Logger &log,
        SubsystemsController *subsystemsController);

    IntermediateNodeExchangePaymentTransaction(
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

    BaseAddress::Shared coordinatorAddress() const override;

    const string logHeader() const override;

protected:
    TransactionResult::SharedConst runPreviousNeighborRequestProcessingStage();
    TransactionResult::SharedConst runCoordinatorRequestProcessingStage();
    TransactionResult::SharedConst runNextNeighborResponseProcessingStage();
    TransactionResult::SharedConst runFinalPathConfigurationProcessingStage();
    TransactionResult::SharedConst runReservationProlongationStage();
    TransactionResult::SharedConst runFinalAmountsConfigurationConfirmation();
    TransactionResult::SharedConst runFinalReservationsCoordinatorConfirmation();
    TransactionResult::SharedConst runFinalReservationsNeighborConfirmation();
    TransactionResult::SharedConst runCheckObservingBlockNumber();
    TransactionResult::SharedConst runVotesCheckingStageWithCoordinatorClarification();

    TransactionResult::SharedConst approve() override;
    void savePaymentOperationIntoHistory(IOTransaction::Shared ioTransaction) override;
    bool checkReservationsDirections() const override;

    void shortageReservationsOnPath(
        const PathID pathID,
        const SerializedEquivalent equivalent,
        const TrustLineAmount &amount);

    TransactionResult::SharedConst sendErrorMessageOnCoordinatorRequest(
        ResponseMessage::OperationState errorState);

    TransactionResult::SharedConst sendErrorMessageOnPreviousNodeRequest(
        BaseAddress::Shared previousNode,
        PathID pathID,
        ResponseMessage::OperationState errorState);

    TransactionResult::SharedConst sendErrorMessageOnNextNodeResponse(
        ResponseMessage::OperationState errorState);

    void sendErrorMessageOnFinalAmountsConfiguration();

private:
    // message on which current transaction was started
    IntermediateNodeReservationRequestMessage::ConstShared mMessage;

    TrustLineAmount mLastReservedAmount;
    Contractor::Shared mCoordinator;
    // id of path, which was processed last
    PathID mLastProcessedPath;
    ExchangeRatesManager *mExchangeRatesManager;
    CommissionsManager *mCommissionsManager;
};

#endif //VTCPD_INTERMEDIATENODEEXCHANGEPAYMENTTRANSACTION_H
