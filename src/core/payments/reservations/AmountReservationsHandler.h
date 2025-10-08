#ifndef VTCPD_AMOUNTBLOCKSHANDLER_H
#define VTCPD_AMOUNTBLOCKSHANDLER_H

#include "AmountReservation.h"
#include "../../common/exceptions/MemoryError.h"
#include "../../common/exceptions/ValueError.h"
#include "../../common/exceptions/NotFoundError.h"
#include "../../transactions/transactions/base/TransactionUUID.h"

#include <map>

class AmountReservationsHandler
{
public:
    AmountReservation::ConstShared reserve(
        ContractorID trustLineContractor,
        const TransactionUUID &transactionUUID,
        const TrustLineAmount &amount,
        const AmountReservation::ReservationDirection direction,
        const SerializedEquivalent equivalent);

    AmountReservation::ConstShared updateReservation(
        ContractorID trustLineContractor,
        const AmountReservation::ConstShared reservation,
        const TrustLineAmount &newAmount,
        const SerializedEquivalent equivalent);

    void free(
        ContractorID trustLineContractor,
        const AmountReservation::ConstShared reservation,
        const SerializedEquivalent equivalent);

    ConstSharedTrustLineAmount totalReserved(
        ContractorID trustLineContractor,
        const AmountReservation::ReservationDirection direction,
        const SerializedEquivalent equivalent,
        const TransactionUUID *transactionUUID = nullptr) const;

    bool isReservationsPresentWithContractor(
        ContractorID trustLineContractorID) const;

    const vector<AmountReservation::ConstShared> contractorReservations(
        ContractorID contractorID,
        const AmountReservation::ReservationDirection direction) const;

    bool isTransactionReservationsPresent(
        const TransactionUUID& transactionUUID) const;

    AmountReservation::ConstShared getReservation(
        ContractorID trustLineContractor,
        const TransactionUUID &transactionUUID,
        const TrustLineAmount &amount,
        const AmountReservation::ReservationDirection direction,
        const SerializedEquivalent equivalent);

protected:
    // One trust line may hold several amount reservations,
    // so the vector<AmountReservation::ConstShared> is used (reservations container).
    //
    // Reservations container would be requested from the map very often,
    // but map's iterator returns copy of the object,
    // so the unique_ptr<vector> is used to get the container without copying it.
    map<ContractorID, unique_ptr<vector<AmountReservation::ConstShared>>> mReservations;

protected:
    std::vector<AmountReservation::ConstShared> reservations(
        ContractorID trustLineContractor,
        const TransactionUUID *transactionUUID = nullptr) const;
};


#endif //VTCPD_AMOUNTBLOCKSHANDLER_H
