#include "AmountReservation.h"


AmountReservation::AmountReservation(
    const TransactionUUID &transactionUUID,
    const TrustLineAmount &amount,
    const ReservationDirection direction,
    const SerializedEquivalent equivalent):

    mTransactionUUID(transactionUUID),
    mBlockedAmount(amount),
    mDirection(direction),
    mEquivalent(equivalent)
{}

const TrustLineAmount &AmountReservation::amount() const
{
    return mBlockedAmount;
}

const TransactionUUID &AmountReservation::transactionUUID() const
{
    return mTransactionUUID;
}

const AmountReservation::ReservationDirection AmountReservation::direction() const
{
    return mDirection;
}

const SerializedEquivalent& AmountReservation::equivalent() const
{
    return mEquivalent;
}

void AmountReservation::operator=(const AmountReservation &rhs)
{
    mBlockedAmount = rhs.mBlockedAmount;
    mTransactionUUID = rhs.mTransactionUUID;
    mDirection = rhs.mDirection;
    mEquivalent = rhs.mEquivalent;
}

bool AmountReservation::operator==(const AmountReservation &rhs) const
{
    return mBlockedAmount == rhs.mBlockedAmount &&
           mTransactionUUID == rhs.mTransactionUUID &&
           mDirection == rhs.mDirection &&
           mEquivalent == rhs.mEquivalent;
}

bool AmountReservation::operator!=(const AmountReservation &rhs) const
{
    return !(rhs == *this);
}
