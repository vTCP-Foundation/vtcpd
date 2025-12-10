#ifndef VTCPD_INTERMEDIATENODEEXCHANGEPAYMENTTRANSACTION_H
#define VTCPD_INTERMEDIATENODEEXCHANGEPAYMENTTRANSACTION_H

#include "base/BaseExchangePaymentTransaction.h"
#include "../../../common/exceptions/RuntimeError.h"
#include "../../../rates/manager/ExchangeRatesManager.h"
#include "../../../rates/manager/CommissionsManager.h"
#include "../../../network/messages/payments/FinalPathExchangeConfigurationMessage.h"

#include <map>
#include <optional>
#include <set>

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
        ExchangePathsManager *exchangePathsManager,
        ObservingHandler *observingHandler,
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
        ExchangePathsManager *exchangePathsManager,
        ObservingHandler *observingHandler,
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

    void shortageIncomingReservationsOnPath(
        const PathID pathID,
        const SerializedEquivalent equivalent,
        const TrustLineAmount &amount);

    void shortageOutgoingReservationsOnPath(
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

    // Sends CoordinatorReservationResponseMessage::RejectedDueConditionsChanged with optional actual values.
    TransactionResult::SharedConst sendRejectedDueConditionsChanged(
        const std::optional<ExchangeRate::Shared> &actualExchangeRate,
        const std::optional<TrustLineAmount> &actualCommission);

    // Answers whether the transaction may finish (no reservations and no stored context).
    bool canCompleteTransaction() const;

    // Clears context caches once all reservations are gone to keep state consistent.
    void clearContextIfTransactionIdle();

private:
    static const uint32_t kMaxTransactionDurationSeconds = 900;

private:
    // message on which current transaction was started
    IntermediateNodeReservationRequestMessage::ConstShared mMessage;

    TrustLineAmount mLastReservedAmount;
    Contractor::Shared mCoordinator;
    // id of path, which was processed last
    PathID mLastProcessedPath;
    ExchangeRatesManager *mExchangeRatesManager;
    CommissionsManager *mCommissionsManager;

    // Stores first-seen exchange rates for (incoming, outgoing) equivalent pairs.
    std::map<std::pair<SerializedEquivalent, SerializedEquivalent>, ExchangeRate::Shared> mContextExchangeRates;
    // Stores first-seen commissions keyed by equivalent to ensure single-charge semantics.
    std::map<SerializedEquivalent, Commission::Shared> mContextCommissions;
    // Tracks equivalents for which the commission was already charged in this payment.
    std::set<SerializedEquivalent> mChargedCommissionEquivalents;

    DateTime mCreationTime;
};

#endif //VTCPD_INTERMEDIATENODEEXCHANGEPAYMENTTRANSACTION_H
