#include "IntermediateNodeExchangePaymentTransaction.h"

IntermediateNodeExchangePaymentTransaction::IntermediateNodeExchangePaymentTransaction(
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
        BaseTransaction::IntermediateNodePaymentTransaction,
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

IntermediateNodeExchangePaymentTransaction::IntermediateNodeExchangePaymentTransaction(
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

bool IntermediateNodeExchangePaymentTransaction::updateReservations(
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

bool IntermediateNodeExchangePaymentTransaction::checkReservationsDirections() const
{
    debug() << "checkReservationsDirections";

    // TODO: After task 06-05 (AmountReservation with equivalent):
    // - Determine single outgoing equivalent using reservation->equivalent()
    // - Convert incoming amounts to outgoing equivalent using ExchangeRatesManager
    // - Deduct commissions for same-equivalent transit using CommissionsManager
    // - Validate total incoming (converted + commission-adjusted) == total outgoing
    // For now: simple validation that both incoming and outgoing exist

    TrustLineAmount totalIncoming = TrustLineAmount(0);
    TrustLineAmount totalOutgoing = TrustLineAmount(0);

    for (const auto& [contractorID, reservations] : mReservations) {
        for (const auto& [pathID, reservation] : reservations) {
            if (reservation->direction() == AmountReservation::Incoming) {
                totalIncoming = totalIncoming + reservation->amount();
            } else if (reservation->direction() == AmountReservation::Outgoing) {
                totalOutgoing = totalOutgoing + reservation->amount();
            }
        }
    }

    debug() << "Total incoming: " << totalIncoming << ", total outgoing: " << totalOutgoing;

    // Return true if we have both incoming and outgoing
    return totalIncoming > TrustLineAmount(0) && totalOutgoing > TrustLineAmount(0);
}
