#include "ReceiverExchangePaymentTransaction.h"

ReceiverExchangePaymentTransaction::ReceiverExchangePaymentTransaction(
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    StorageHandler *storageHandler,
    ResourcesManager *resourcesManager,
    ExchangeRatesManager *exchangeRatesManager,
    CommissionsManager *commissionsManager,
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
        subsystemsController),
    mExchangeRatesManager(exchangeRatesManager),
    mCommissionsManager(commissionsManager)
{
}

ReceiverExchangePaymentTransaction::ReceiverExchangePaymentTransaction(
    BytesShared buffer,
    ContractorsManager *contractorsManager,
    EquivalentsSubsystemsRouter *equivalentsSubsystemsRouter,
    StorageHandler *storageHandler,
    ResourcesManager *resourcesManager,
    ExchangeRatesManager *exchangeRatesManager,
    CommissionsManager *commissionsManager,
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
        subsystemsController),
    mExchangeRatesManager(exchangeRatesManager),
    mCommissionsManager(commissionsManager)
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

bool ReceiverExchangePaymentTransaction::updateReservations(
    const vector<PathReservation> &finalAmounts)
{
    debug() << "updateReservations";

    // TODO: After task 06-05 (AmountReservation with equivalent):
    // - Use reservation->equivalent() to validate equivalent matches
    // - Return false if PathID matches but equivalent differs
    // For now: simple PathID + amount validation without equivalent check

    unordered_set<PathID> updatedPaths;
    const auto reservationsCopy = mReservations;

    for (const auto &nodeAndReservations : reservationsCopy) {
        for (auto pathIDAndReservation : nodeAndReservations.second) {
            bool found = false;
            PathID matchedPathID = std::numeric_limits<PathID>::max();

            // Find matching final amount by pathID
            for (const auto &finalAmount : finalAmounts) {
                if (finalAmount.pathID == pathIDAndReservation.first) {
                    // Found matching pathID
                    if (*finalAmount.amount.get() != pathIDAndReservation.second->amount()) {
                        shortageReservation(
                            nodeAndReservations.first,
                            pathIDAndReservation.second,
                            *finalAmount.amount.get(),
                            finalAmount.pathID,
                            finalAmount.equivalent);
                    }
                    matchedPathID = finalAmount.pathID;
                    found = true;
                    break;
                }
            }

            if (found) {
                updatedPaths.insert(matchedPathID);
            } else {
                // Reservation with pathID not in finalAmounts - drop it
                dropNodeReservationsOnPath(pathIDAndReservation.first);
            }
        }
    }

    return updatedPaths.size() == finalAmounts.size();
}

bool ReceiverExchangePaymentTransaction::checkReservationsDirections() const
{
    debug() << "checkReservationsDirections";

    // TODO: After task 06-05 (AmountReservation with equivalent):
    // - Validate all incoming reservations are in mEquivalent using reservation->equivalent()
    // For now: simple validation that incoming reservations exist

    TrustLineAmount totalIncoming = TrustLineAmount(0);
    bool hasIncoming = false;

    for (const auto& [contractorID, reservations] : mReservations) {
        for (const auto& [pathID, reservation] : reservations) {
            if (reservation->direction() == AmountReservation::Incoming) {
                totalIncoming = totalIncoming + reservation->amount();
                hasIncoming = true;
            }
        }
    }

    // Validate we have incoming reservations
    return hasIncoming && totalIncoming > TrustLineAmount(0);
}
