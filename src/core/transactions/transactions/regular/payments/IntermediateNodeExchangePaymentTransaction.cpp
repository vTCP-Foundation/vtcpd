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

bool IntermediateNodeExchangePaymentTransaction::checkReservationsDirections() const
{
    debug() << "checkReservationsDirections";

    // Step 1: Determine outgoing equivalent and validate uniformity
    SerializedEquivalent outgoingEquivalent;
    bool outgoingEquivalentSet = false;
    TrustLineAmount totalOutgoing = TrustLineAmount(0);

    for (const auto& [contractorID, reservations] : mReservations) {
        for (const auto& [pathID, reservation] : reservations) {
            if (reservation->direction() == AmountReservation::Outgoing) {
                if (!outgoingEquivalentSet) {
                    outgoingEquivalent = reservation->equivalent();
                    outgoingEquivalentSet = true;
                } else if (outgoingEquivalent != reservation->equivalent()) {
                    // All outgoing must be in same equivalent
                    warning() << "checkReservationsDirections: Multiple outgoing equivalents detected. "
                              << "First: " << outgoingEquivalent << ", found: " << reservation->equivalent();
                    return false;
                }
                totalOutgoing = totalOutgoing + reservation->amount();
            }
        }
    }

    if (!outgoingEquivalentSet) {
        // No outgoing reservations - invalid for intermediate node
        warning() << "checkReservationsDirections: No outgoing reservations found";
        return false;
    }

    // Step 2: Calculate total incoming converted to outgoing equivalent
    TrustLineAmount totalIncomingConverted = TrustLineAmount(0);
    set<SerializedEquivalent> processedIncomingEquivalents;

    for (const auto& [contractorID, reservations] : mReservations) {
        for (const auto& [pathID, reservation] : reservations) {
            if (reservation->direction() == AmountReservation::Incoming) {
                SerializedEquivalent incomingEquiv = reservation->equivalent();
                TrustLineAmount incomingAmount = reservation->amount();

                if (incomingEquiv == outgoingEquivalent) {
                    // Same equivalent - direct add
                    totalIncomingConverted = totalIncomingConverted + incomingAmount;

                    // Step 3: Deduct commission once per incoming equivalent (transit only)
                    if (processedIncomingEquivalents.find(incomingEquiv) == processedIncomingEquivalents.end()) {
                        auto commission = mCommissionsManager->getCommission(incomingEquiv);
                        if (commission) {
                            TrustLineAmount commissionAmount(commission->amount());
                            totalIncomingConverted = totalIncomingConverted - commissionAmount;
                            debug() << "Deducted commission for equivalent " << incomingEquiv
                                    << ": " << commissionAmount;
                        }
                        processedIncomingEquivalents.insert(incomingEquiv);
                    }
                } else {
                    // Different equivalent - convert
                    auto rate = mExchangeRatesManager->get(incomingEquiv, outgoingEquivalent);
                    if (!rate) {
                        // No exchange rate found
                        warning() << "checkReservationsDirections: No exchange rate found for "
                                  << incomingEquiv << " -> " << outgoingEquivalent;
                        return false;
                    }

                    try {
                        TrustLineAmount converted = mExchangeRatesManager->calculateConvertedAmount(
                            incomingEquiv, outgoingEquivalent, incomingAmount);
                        totalIncomingConverted = totalIncomingConverted + converted;
                        debug() << "Converted " << incomingAmount << " from equiv " << incomingEquiv
                                << " to " << converted << " in equiv " << outgoingEquivalent;
                    } catch (const Exception& e) {
                        // Conversion failed (overflow or other error)
                        warning() << "checkReservationsDirections: Conversion failed: " << e.what();
                        return false;
                    }

                    // No commission for exchange operations
                }
            }
        }
    }

    // Step 4: Compare totals
    debug() << "Total incoming (converted): " << totalIncomingConverted
            << ", total outgoing: " << totalOutgoing;

    return totalIncomingConverted == totalOutgoing;
}
