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

    unordered_set<PathID> updatedPaths;
    const auto reservationsCopy = mReservations;

    for (const auto &nodeAndReservations : reservationsCopy) {
        for (auto pathIDAndReservation : nodeAndReservations.second) {
            bool found = false;
            PathID matchedPathID = std::numeric_limits<PathID>::max();

            // Find matching final amount by pathID AND equivalent
            for (const auto &finalAmount : finalAmounts) {
                if (finalAmount.pathID == pathIDAndReservation.first) {
                    // Validate equivalent matches
                    if (finalAmount.equivalent != pathIDAndReservation.second->equivalent()) {
                        warning() << "updateReservations: PathID " << finalAmount.pathID
                                  << " equivalent mismatch. Expected: "
                                  << pathIDAndReservation.second->equivalent()
                                  << ", got: " << finalAmount.equivalent;
                        return false;
                    }

                    // Found matching pathID with correct equivalent
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

    TrustLineAmount totalIncoming = TrustLineAmount(0);
    bool hasIncoming = false;

    for (const auto& [contractorID, reservations] : mReservations) {
        for (const auto& [pathID, reservation] : reservations) {
            if (reservation->direction() == AmountReservation::Incoming) {
                // Validate all incoming reservations are in receiver's equivalent (mEquivalent)
                if (reservation->equivalent() != mEquivalent) {
                    warning() << "checkReservationsDirections: Incoming reservation in wrong equivalent. "
                              << "Expected: " << mEquivalent << ", got: " << reservation->equivalent();
                    return false;
                }
                totalIncoming = totalIncoming + reservation->amount();
                hasIncoming = true;
            }
        }
    }

    // Validate we have incoming reservations
    return hasIncoming && totalIncoming > TrustLineAmount(0);
}
